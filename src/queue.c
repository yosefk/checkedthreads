#include "queue.h"
#include "atomic.h"
#include <stdio.h>

int ct_atomic_add_modulo(volatile int* p, int incr, int what) {
    int curr, next;
    do {
        curr = *p;
        next = (curr + incr) % what;
    } while(!ATOMIC_COMPARE_AND_SWAP(p, curr, next));
    return curr;
}

int ct_enqueue(ct_work_queue* q, ct_work_item* item) {
    /* queue is full: */
    if(q->size == q->capacity) {
        return 0;
    }

    /* if the queue was almost full, this will "deter" other producers.
       since size serves no other purpose, it's not a problem that
       we expose a state of size which is inconsistent with the
       difference between read_ind and write_ind.
     */
    ATOMIC_FETCH_THEN_INCR(&q->size, 1);

    /* we need to atomically increment the write index modulo size: */
    q->work_items[ct_atomic_add_modulo(&q->write_ind, 1, q->capacity)] = item;

    return 1;
}

ct_work_item* ct_dequeue(ct_work_queue* q) {
    int read_ind;

    /* queue is empty: */
    if(q->size == 0) {
        return 0;
    }

    /* atomically increment the read index modulo size: */
    read_ind = ct_atomic_add_modulo(&q->read_ind, 1, q->capacity);

    /* now when we have our own read index, decrement queue size so
       that producers can enqueue a work item (only matters if queue
       was full): */
    ATOMIC_FETCH_THEN_DECR(&q->size, 1);

    return q->work_items[read_ind];
}

void ct_work_until_done(ct_work_item* item, int quit_if_no_work) {
    int n = item->n;
    ct_ind_func f = item->f;
    void* context = item->context;
    while(item->to_do > 0) {
        int next_ind = item->next_ind;
        if(next_ind < n) { /* we don't want to busily increment it
                              when it exceeds n but we still wait for someone. */
            next_ind = ATOMIC_FETCH_THEN_INCR(&item->next_ind, 1);
            if(next_ind < n) { /* it could have exceeded n because of the concurrent increment above */
                f(next_ind, context);
                ATOMIC_FETCH_THEN_DECR(&item->to_do, 1);
            }
        }
        else if(quit_if_no_work) {
            return;
        }
    }
}

int ct_do_some_work(ct_work_item* item) {
    int n = item->n;
    int next_ind = item->next_ind;
    if(next_ind < n) {
        next_ind = ATOMIC_FETCH_THEN_INCR(&item->next_ind, 1);
        if(next_ind < n) {
            ct_ind_func f = item->f;
            void* context = item->context;
            f(next_ind, context);
            ATOMIC_FETCH_THEN_DECR(&item->to_do, 1);
        }
    }
    return item->to_do > 0;
}
