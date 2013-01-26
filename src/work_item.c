#include "work_item.h"
#include "atomic.h"

void ct_work(ct_work_item* item) {
    int n = item->n;
    ct_ind_func f = item->f;
    void* context = item->context;
    while(item->next_ind < n) {
        int next_ind = ATOMIC_FETCH_THEN_INCR(&item->next_ind, 1);
        if(next_ind < n) { /* it could have exceeded n because of the concurrent increment above */
            ct_canceller* canceller = item->canceller;
            if(canceller && canceller->cancelled) {
                item->to_do = 0; /* note that this can bring to_do to a negative value
                                    because of concurrent decrements; which is OK. */
                item->next_ind = n; /* OK similarly to to_do above. */
                break;
            }
            f(next_ind, context);
            ATOMIC_FETCH_THEN_DECR(&item->to_do, 1);
        }
    }
}
