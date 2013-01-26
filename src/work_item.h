#ifndef CT_WORK_ITEM_H_
#define CT_WORK_ITEM_H_

#include "imp.h"

typedef struct {
    volatile int next_ind;
    volatile int to_do;
    volatile int n;
    volatile int ref_cnt;
    ct_ind_func f;
    void* context;
    ct_canceller* volatile canceller;
} ct_work_item;

/* returns when next_ind reaches or exceeds n - all work was already yanked.
   this doesn't mean we're done - to_do==0 means that. */
void ct_work(ct_work_item* item);

#endif

