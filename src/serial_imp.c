#include "imp.h"

void ct_serial_init(const ct_env_var* env) {
    (void)env;
}

void ct_serial_fini(void) {
}

void ct_serial_for(int n, ct_ind_func f, void* context, ct_canceller* c) {
    int i;
    for(i=0; i<n; ++i) {
        if(c->cancelled) {
            break;
        }
        f(i, context);
    }
}

ct_imp g_ct_serial_imp = {
    "serial",
    &ct_serial_init,
    &ct_serial_fini,
    &ct_serial_for,
    0, 0, 0, /* cancelling functions */
};
