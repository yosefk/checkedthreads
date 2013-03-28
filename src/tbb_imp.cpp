#include "imp.h"

#ifdef CT_TBB

#include <tbb/tbb.h>

void ctx_tbb_init(const ct_env_var* env) {
    int num_threads = atoi(ct_getenv(env, "CT_THREADS", "0"));
    if(num_threads) {
        static tbb::task_scheduler_init init(num_threads);
    }
    else {
        static tbb::task_scheduler_init init;
    }
}

void ctx_tbb_fini(void) {
}

struct ctx_invoker {
    ct_ind_func f;
    void* context;
    ct_canceller* canceller;

    void operator()(const tbb::blocked_range<int>& range) const {
        int begin = range.begin();
        int end = range.end();
        for(int i=begin; i<end; ++i) {
            if(canceller->cancelled) {
                tbb::task::self().cancel_group_execution();
                return;
            }
            else {
                f(i, context);
            }
        }
    }
};

void ctx_tbb_for(int n, ct_ind_func f, void* context, ct_canceller* c) {
    ctx_invoker invoker;
    invoker.f=f;
    invoker.context=context;
    invoker.canceller=c;

    /* grain_size=1 [it's blocked_range's default but why take chances...]
       and simple_partitioner which presumably uses "chunk size=grain size"
       should result in per-index dynamic partitioning: that is, indexes
       are logically yanked from a shared queue one by one, so there's never
       a thread who got stuck with two heavy indexes while others are free
       and can't steal its hard work because it happened to be assigned
       into a single "indivisible" chunk.

       a plain parallel_for(0, n, callback) was observed to /not/ do this;
       that is, a thread does get stuck with heavy work which is not stolen.
       */
    int grain_size = 1;
    tbb::task_group_context ctx(tbb::task_group_context::isolated);
    tbb::parallel_for(tbb::blocked_range<int>(0, n, grain_size), invoker,
                      tbb::simple_partitioner(), ctx);
}

ct_imp g_ct_tbb_imp = {
    "tbb",
    &ctx_tbb_init,
    &ctx_tbb_fini,
    &ctx_tbb_for,
    0, 0, 0, /* cancelling functions */
};

#else

ct_imp g_ct_tbb_imp;

#endif
