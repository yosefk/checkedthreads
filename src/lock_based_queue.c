#include "lock_based_queue.h"
#include "atomic.h"

void ct_locked_queue_init(ct_locked_queue* q, ct_work_item** work_items, int capacity) {
    pthread_mutex_init(&q->mutex, 0);
    q->work_items = work_items;
    q->capacity = capacity;
    q->read_ind = 0;
    q->write_ind = 0;
    q->size = 0;
}

int ct_locked_enqueue(ct_locked_queue* q, ct_work_item* item, int reps) {
    int ret = 0, capacity;
    /* don't bother to lock if the queue is full */
    if(q->size + reps > q->capacity) {
        return ret;
    }
    pthread_mutex_lock(&q->mutex);
    /* check again if we aren't full, this time under a lock
       (so that if we aren't full, we can count on staying not full
       inside the if) */
    capacity = q->capacity;
    if(q->size + reps <= capacity) {
        int i, w=q->write_ind;
        for(i=0; i<reps; ++i) {
            q->work_items[w] = item;
            w++;
            if(w >= capacity) {
                w = 0;
            }
        }
        q->write_ind = w;
        q->size += reps;
        ret = 1;
    }
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

ct_work_item* ct_locked_dequeue(ct_locked_queue* q) {
    ct_work_item* ret = 0;
    /* don't bother to lock if the queue is empty */
    if(q->size == 0) {
        return ret;
    }
    pthread_mutex_lock(&q->mutex);
    /* check again if we're empty */
    if(q->size > 0) {
        int r = q->read_ind;
        ret = q->work_items[r];
        r++;
        if(r >= q->capacity) {
            r = 0;
        }
        q->read_ind = r;
        q->size -= 1;
    }
    pthread_mutex_unlock(&q->mutex);
    return ret;
}
