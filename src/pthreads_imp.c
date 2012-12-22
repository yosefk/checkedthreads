#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "imp.h"
#include "atomic.h"

typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    pthread_t* threads;
    int num_threads;
    volatile int num_initialized;
    int work_available;
    int terminate;
    volatile int next_ind;
    volatile int to_do;
    volatile int n;
    ct_ind_func f;
    void* context;
} ct_pthread_pool;

ct_pthread_pool g_ct_pthread_pool = {
    PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* keep yanking jobs (atomic next_ind++ to get next,
   atomic to_do-- to report that it's done) while
   there are jobs available (to_do != 0) */
void ct_pthreads_yank_while_not_done(void) {
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    int n = pool->n;
    ct_ind_func f = pool->f;
    void* context = pool->context;
    while(pool->to_do) {
        int next_ind = pool->next_ind;
        if(next_ind < n) { /* we don't want to busily increment it
                              when it exceeds n but we still wait for someone. */
            next_ind = ATOMIC_FETCH_THEN_INCR(&pool->next_ind, 1);
            if(next_ind < n) {
                f(next_ind, context);
                ATOMIC_FETCH_THEN_DERC(&pool->to_do, 1);
            }
        }
    }
}

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
            pthread_mutex_unlock(&pool->mutex);
            /* the master also calls this, and so we all sync using the to_do
               counter - when it reaches 0, we all quit ct_pthreads_yank_while_not_done() */
            ct_pthreads_yank_while_not_done();
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
    int i;
    /* TODO: we should figure out #cores instead. */
    /* TODO: we might want a way to get the threads for the pool from the outside. */
    /* TODO: what should num_threads mean - including the master or not? what
       does it mean in TBB, OpenMP, etc.? */
    if(num_threads == 0) {
        printf("checkedthreads - WARNING: $CT_THREADS not set to non-zero value; using 2 pthreads.\n");
        num_threads = 2;
    }
    pthread_cond_init(&pool->cond, 0);
    pthread_mutex_init(&pool->mutex, 0);
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*num_threads);
    pool->num_threads = num_threads;
    for(i=0; i<num_threads; ++i) {
        pthread_create(&pool->threads[i], 0, ct_pthreads_worker, (void*)(size_t)i);
        /* wait for the spawned thread to lock the cond var mutex */
        while(pool->num_initialized == i);
        /* now make sure it /unlocked/ the mutex - that is, that it entered the wait */
        pthread_mutex_lock(&pool->mutex);
        pthread_mutex_unlock(&pool->mutex);
    }
}

void ct_pthreads_fini(void) {
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    pool->terminate = 1;
    ct_pthreads_broadcast();
}

void ct_pthreads_for(int n, ct_ind_func f, void* context) {
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    pool->n = n;
    pool->to_do = n;
    pool->next_ind = 0;
    pool->f = f;
    pool->context = context;

    pool->work_available = 1;
    ct_pthreads_broadcast();
    ct_pthreads_yank_while_not_done();
    pool->work_available = 0;
}

ct_imp g_ct_pthreads_imp = {
    "pthreads",
    &ct_pthreads_init,
    &ct_pthreads_fini,
    &ct_pthreads_for,
};
