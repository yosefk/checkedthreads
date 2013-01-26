#include "imp.h"

#ifdef CT_OPENMP

void ct_openmp_init(const ct_env_var* env) {
    (void)env;
}

void ct_openmp_fini(void) {
}

void ct_openmp_for(int n, ct_ind_func f, void* context, ct_canceller* c) {
    int i;
    int cancelled = 0;
#pragma omp parallel for schedule(dynamic,1)
    for(i=0; i<n; ++i) {
        if(!cancelled && !c->cancelled) {
          f(i, context);
          cancelled = c->cancelled;
        }
    }
}

ct_imp g_ct_openmp_imp = {
    "openmp",
    &ct_openmp_init,
    &ct_openmp_fini,
    &ct_openmp_for,
    0, 0, 0, /* cancelling functions */
};

#else

ct_imp g_ct_openmp_imp;

#endif
