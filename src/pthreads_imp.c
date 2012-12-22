#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "imp.h"

/* handling of cond vars/signal/wait is as described here:
http://stackoverflow.com/questions/2763714/why-do-pthreads-condition-variable-functions-require-a-mutex
(in particular, the page discusses spurious wakeups and advises to have a
"work available" flag to be checked after waiting for a cond var.)
*/

typedef struct {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    pthread_t* threads;
    int num_threads;
    volatile int num_initialized;
    int work_available;
    int terminate;
    int n;
    ct_ind_func f;
    void* context;
} ct_pthread_pool;

ct_pthread_pool g_ct_pthread_pool = {
    PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
    0, 0, 0, 0, 0, 0, 0, 0
};

volatile int g_fixme_done[2];

void* ct_pthreads_worker(void* arg) {
    int id = (int)(size_t)arg;
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    pthread_mutex_lock(&pool->mutex);
    ++pool->num_initialized; /* this signals the master that it should
                                sync with us by locking and unlocking
                                the cond var mutex */
    while(!pool->terminate) {
        /* wait unlocks the mutex while it waits... */
        pthread_cond_wait(&pool->cond, &pool->mutex);
        /* ...and locks it back before it returns. */

        /* if !work_available, it's a spurious wakeup - simply wait again. */
        if(pool->work_available) {
            int i;
            int single_share = pool->n / pool->num_threads;
            pthread_mutex_unlock(&pool->mutex);
            /* do the work; FIXME: yank indexes from a logical shared queue. */
            for(i=single_share*id; i<single_share*(id+1); ++i) {
                pool->f(i, pool->context);
            }
            g_fixme_done[id] = 1;
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
    /* TODO: terminate the threads */
}

void ct_pthreads_for(int n, ct_ind_func f, void* context) {
    ct_pthread_pool* pool = &g_ct_pthread_pool;
    pool->n = n;
    pool->f = f;
    pool->context = context;
    pool->work_available = 1;
    ct_pthreads_broadcast();
    /* FIXME */
    while(!g_fixme_done[0] || !g_fixme_done[1]);
    pool->work_available = 0;
}

ct_imp g_ct_pthreads_imp = {
    "pthreads",
    &ct_pthreads_init,
    &ct_pthreads_fini,
    &ct_pthreads_for,
};
