#include "imp.h"
#include "queue.h" /* FIXME */
#include "lock_based_queue.h"

#ifdef CT_PTHREADS

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "atomic.h"

typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    ct_locked_queue q;
    pthread_t* threads;
    int num_threads;
    volatile int num_initialized;
    int work_available;
    int terminate;
} ct_pthread_pool;

/* TODO: allocate dynamically with an option to set size from environment? */
#define MAX_ITEMS (8*1024)
ct_work_item* g_ct_pthread_items[MAX_ITEMS];

ct_pthread_pool g_ct_pthread_pool = {
    PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    CT_LOCKED_QUEUE_INITIALIZER,
    0, 0, 0, 0, 0
};

void* ct_pthreads_worker(void* arg) {
    int id = (int)(size_t)arg;
    ct_pthread_pool* pool = &g_ct_pthread_pool;

    (void)id; /* TODO: use this to implement a ct_curr_thread() function
                 (thread-local storage doesn't require an ID number - there are pthread keys for that. */

    pthread_mutex_lock(&pool->mutex);
    ++pool->num_initialized; /* this signals the master that it should
                                sync with us by locking and unlocking
                                the cond var mutex */
    while(!pool->terminate) {
        /* wait unlocks the mutex while it waits... */
        pthread_cond_wait(&pool->cond, &pool->mutex);
        /* ...and locks it back before it returns. */

        /* if !work_available (and !terminate),
           it's a spurious wakeup - simply wait again. */
        if(pool->work_available) {
            ct_work_item* item;
            pthread_mutex_unlock(&pool->mutex);
            /* the master also calls this, and so we all sync using the to_do
               counter - when it reaches 0, we all quit ct_pthreads_yank_while_not_done() */
            do {
                item = ct_locked_dequeue(&pool->q);
                if(item) {
                    ct_work_until_done(item, 1);
                }
            } while(item);
            /* lock the mutex back before waiting */
            pthread_mutex_lock(&pool->mutex);
        }
    }
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

void ct_pthreads_broadcast(void)
{
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    pthread_mutex_lock(&pool->mutex);
    pthread_cond_broadcast(&pool->cond);
    pthread_mutex_unlock(&pool->mutex);
}

void ct_pthreads_init(int num_threads) {
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    pthread_attr_t attr;
    int i;
    /* TODO: we should figure out #cores instead. */
    /* TODO: we might want a way to get the threads for the pool from the outside. */
    /* TODO: what should num_threads mean - including the master or not? what
       does it mean in TBB, OpenMP, etc.? */
    if(num_threads == 0) {
        printf("checkedthreads - WARNING: $CT_THREADS not set to non-zero value; using 2 pthreads.\n");
        num_threads = 2;
    }
    /* here, num_threads means "number of slaves", whereas $CT_THREADS is the total number,
       including the master */
    num_threads--;

    pthread_cond_init(&pool->cond, 0);
    pthread_mutex_init(&pool->mutex, 0);
    ct_locked_queue_init(&pool->q, g_ct_pthread_items, MAX_ITEMS);
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*num_threads);
    pool->num_threads = num_threads;
    /* For portability, explicitly create threads in a joinable state.
       -- https://computing.llnl.gov/tutorials/pthreads/#ConditionVariables */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    for(i=0; i<num_threads; ++i) {
        pthread_create(&pool->threads[i], &attr, ct_pthreads_worker, (void*)(size_t)i);
        /* wait for the spawned thread to lock the cond var mutex */
        while(pool->num_initialized == i);
        /* now make sure it /unlocked/ the mutex - that is, that it entered the wait */
        pthread_mutex_lock(&pool->mutex);
        pthread_mutex_unlock(&pool->mutex);
    }
    pthread_attr_destroy(&attr);
}

void ct_pthreads_fini(void) {
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    int i;
    pool->terminate = 1;
    ct_pthreads_broadcast();
    for(i=0; i<pool->num_threads; ++i) {
        pthread_join(pool->threads[i], 0);
    }
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
}

void ct_pthreads_for(int n, ct_ind_func f, void* context) {
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    ct_locked_queue* q = &pool->q;
    ct_work_item my_item;
    ct_work_item* item = &my_item;
    int reps = n < pool->num_threads ? n : pool->num_threads;
    int i;

    item->n = n;
    item->to_do = n;
    item->next_ind = 0;
    item->f = f;
    item->context = context;
    item->ref_cnt = 0; /* we don't count... */

    /* try to enqueue the item, and do some work while that fails */
    while(!ct_locked_enqueue(q, item, reps)) {
        --n;
        f(n, context);
        if(n == 0) { /* we're done while waiting... */
            return;
        }
        item->n = n;
        item->to_do = n;
    }

    /* work_available is managed using atomic increments because there may
       be concurrent calls to ct_for and wakeups are only truly spurious
       if work_available reaches zero because no thread has enqueued any work. */
    /* TODO: we don't need work_available if the dequeue function tells us when the q is empty...*/
    ATOMIC_FETCH_THEN_INCR(&pool->work_available, 1);

    /* everybody wake up! we have work for you. */
    ct_pthreads_broadcast();

    /* let's do our share: */
    ct_work_until_done(item, 1);

    while(item->to_do > 0 || item->ref_cnt > 0) {
        ct_work_item* another_item;
        do {
            another_item = ct_locked_dequeue(q);
            if(another_item) {
                ct_work_until_done(another_item, 1);
            }
        } while(another_item);
    }

    ATOMIC_FETCH_THEN_INCR(&pool->work_available, 1);
}

ct_imp g_ct_pthreads_imp = {
    "pthreads",
    &ct_pthreads_init,
    &ct_pthreads_fini,
    &ct_pthreads_for,
};

#else

ct_imp g_ct_pthreads_imp;

#endif

