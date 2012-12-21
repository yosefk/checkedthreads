#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "imp.h"

extern ct_imp g_ct_serial_imp;
extern ct_imp g_ct_valgrind_imp;

ct_imp* g_ct_imps[] = {
    &g_ct_serial_imp,
    &g_ct_valgrind_imp,
    0
};

ct_imp* g_ct_pimpl;
int g_ct_verbose;

const char* ct_getenv(const ct_env_var* env, const char* name, const char* default_value) {
    int i=0;
    const char* got=0;
    if(env) {
        while(env[i].name) {
            if(strcmp(env[i].name, name) == 0) {
                return env[i].value;
            }
            ++i;
        }
    }
    got = getenv(name);
    return got ? got : default_value;
}

ct_imp* ct_sched(const char* name) {
    int i=0;
    while(g_ct_imps[i]) {
        if(strcmp(g_ct_imps[i]->name, name) == 0) {
            return g_ct_imps[i];
        }
        ++i;
    }
    return 0;
}

void ct_init(int num_threads, const ct_env_var* env) {
    const char* default_sched = "serial"; /* FIXME: should be "balance" */
    const char* sched = ct_getenv(env, "CT_SCHED", default_sched);
    g_ct_pimpl = ct_sched(sched);
    if(!g_ct_pimpl) {
        printf("safethreads - WARNING: unknown scheduler (%s) specified, using %s instead\n",
                sched, default_sched);
        g_ct_pimpl = ct_sched(default_sched);
    }

    g_ct_pimpl->imp_init(num_threads);
}

void ct_fini(void) {
    g_ct_pimpl->imp_fini();
    g_ct_pimpl = 0;
}

void ct_for(int n, ct_ind_func f, void* context) {
    g_ct_pimpl->imp_for(n, f, context);
}
