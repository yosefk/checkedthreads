#include "imp.h"

#ifdef CT_OPENMP

void ct_openmp_init(const ct_env_var* env) {
    (void)env;
}

void ct_openmp_fini(void) {
}

void ct_openmp_for(int n, ct_ind_func f, void* context) {
    int i;
    /* we could configure scheduling per loop or per run and
       then there'd be an if and two for loops with different
       scheduling clauses. however, with ct_for, which doesn't
       even have a range callback but an index callback,
       the basic bet is that there's a lot to do per index
       and this "lot" is likely to be variable; while the problem
       of scattering the processing of adjacent data across cores
       is less likely. so perhaps a better interface to a policy
       where large subranges are handled in the same thread would
       be a ct_for variant with a range callback. */
#pragma omp parallel for schedule(dynamic,1)
    for(i=0; i<n; ++i) {
        f(i, context);
    }
}

ct_imp g_ct_openmp_imp = {
    "openmp",
    &ct_openmp_init,
    &ct_openmp_fini,
    &ct_openmp_for,
};

#else

ct_imp g_ct_openmp_imp;

#endif
