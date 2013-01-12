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
} ct_work_item;

#endif

