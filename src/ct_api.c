#include "imp.h"

extern ct_imp g_ct_serial_imp;

ct_imp* g_ct_pimpl;
int g_ct_verbose;

void ct_init(int num_threads, const ct_env_var* env) {
    (void)env;
    g_ct_pimpl = &g_ct_serial_imp;

    g_ct_pimpl->imp_init(num_threads);
}

void ct_fini(void) {
    g_ct_pimpl->imp_fini();
    g_ct_pimpl = 0;
}

void ct_for(int n, ct_ind_func f, void* context) {
    g_ct_pimpl->imp_for(n, f, context);
}
