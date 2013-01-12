/*
 * A lock-based multi-producer, multi-consumer queue implementation.
 */
#ifndef CT_LOCK_BASED_QUEUE_H_
#define CT_LOCK_BASED_QUEUE_H_

#include "work_item.h"
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex; /* currently everything is protected by one mutex */
    ct_work_item** work_items;
    int capacity;
    volatile int read_ind;
    volatile int write_ind;
    volatile int size;
} ct_locked_queue;

#define CT_LOCKED_QUEUE_INITIALIZER { PTHREAD_MUTEX_INITIALIZER, 0, 0, 0, 0, 0 }

void ct_locked_queue_init(ct_locked_queue* q, ct_work_item** work_items, int capacity);
/* a single work item is enqueued reps number of times (or not at all). in terms of lifecycles,
   it means that once the work is done, it won't take long to dequeue the pointers to the work items,
   so we can wait until that happens (the alternative is dynamic allocation). */
int ct_locked_enqueue(ct_locked_queue* q, ct_work_item* item, int reps);
ct_work_item* ct_locked_dequeue(ct_locked_queue* q);

#endif
