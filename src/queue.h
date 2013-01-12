#ifndef CT_QUEUE_H_
#define CT_QUEUE_H_

#include "work_item.h"

typedef struct {
    /* work items are kept by pointer; the idea is that a single pointer is enqueued up to N times,
       so that every thread can then dequeue it and do its share of the work. */
    ct_work_item** work_items;
    volatile int write_ind; /* producer's next task to write is work_items[write_ind] */
    volatile int read_ind; /* consumer's next task to read is work_items[read_ind] */
    int capacity;
    int size;
} ct_work_queue;

/* called by producers. non-zero iff the queue wasn't full and enqueuing succeeded. */
int ct_enqueue(ct_work_queue* q, ct_work_item* item);
/* called by consumers who then do the work (modifying the struct). 0 if queue is empty. */
ct_work_item* ct_dequeue(ct_work_queue* q);

/* effectively, a single work item is itself a queue of indexes shared with other consumers. */
void ct_work_until_done(ct_work_item* item, int quit_if_no_work);
/* this is useful when you're waiting for something that you hope will soon happen, so you want
   to do a small share of work but not too much. non-zero iff work remains for a next call. */
int ct_do_some_work(ct_work_item* item);

#endif
