#include "imp.h"
#include <tbb/tbb.h>

void ctx_tbb_init(int num_threads) {
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

    void operator()(int i) const {
        f(i, context);
    }
};

void ctx_tbb_for(int n, ct_ind_func f, void* context) {
    ctx_invoker invoker;
    invoker.f=f;
    invoker.context=context;
    tbb::parallel_for(0, n, 1, invoker);
}

ct_imp g_ct_tbb_imp = {
    "tbb",
    &ctx_tbb_init,
    &ctx_tbb_fini,
    &ctx_tbb_for,
};
