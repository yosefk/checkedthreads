#include "work_item.h"
#include "atomic.h"

void ct_work(ct_work_item* item) {
    int n = item->n;
    ct_ind_func f = item->f;
    void* context = item->context;
    while(item->next_ind < n) {
        int next_ind = ATOMIC_FETCH_THEN_INCR(&item->next_ind, 1);
        if(next_ind < n) { /* it could have exceeded n because of the concurrent increment above */
            f(next_ind, context);
            ATOMIC_FETCH_THEN_DECR(&item->to_do, 1);
        }
    }
}
