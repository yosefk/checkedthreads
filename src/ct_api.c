#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "imp.h"

extern ct_imp g_ct_tbb_imp;
extern ct_imp g_ct_serial_imp;
extern ct_imp g_ct_openmp_imp;
extern ct_imp g_ct_shuffle_imp;
extern ct_imp g_ct_valgrind_imp;

ct_imp* g_ct_imps[] = {
    &g_ct_tbb_imp,
    &g_ct_serial_imp,
    &g_ct_openmp_imp,
    &g_ct_shuffle_imp,
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

void ct_init(const ct_env_var* env) {
    int num_threads = atoi(ct_getenv(env, "CT_THREADS", "0"));
    const char* default_sched = "openmp";
    const char* sched = ct_getenv(env, "CT_SCHED", default_sched);
    g_ct_pimpl = ct_sched(sched);
    if(!g_ct_pimpl) {
        printf("checkedthreads - WARNING: unknown scheduler (%s) specified, using %s instead\n",
                sched, default_sched);
        g_ct_pimpl = ct_sched(default_sched);
    }
    /* TODO: it'd be nice to warn when verbosity>1 won't really work -
       that is, with truly parallel schedulers. */
    g_ct_verbose = atoi(ct_getenv(env, "CT_VERBOSE", "0"));

    g_ct_pimpl->imp_init(num_threads);
}

void ct_fini(void) {
    g_ct_pimpl->imp_fini();
    g_ct_pimpl = 0;
}

typedef struct {
    ct_ind_func next_func;
    void* next_context;
} ct_wrapped_func_context;

void ct_verbose_ind_func(int index, void* context) {
    ct_wrapped_func_context* wc = (ct_wrapped_func_context*)context;
    printf("checkedthreads: i=%d\n", index);
    wc->next_func(index, wc->next_context);
}

void ct_for(int n, ct_ind_func f, void* context) {
    if(g_ct_verbose>0) {
        /* TODO: add task name */
        ct_wrapped_func_context wc;
        printf("checkedthreads: ct_for(%d) entered\n",n);
        if(g_ct_verbose>1) {
            wc.next_func = f;
            wc.next_context = context;
            f = ct_verbose_ind_func;
            context = &wc;
        }
        g_ct_pimpl->imp_for(n, f, context);
        printf("checkedthreads: ct_for(%d) ended\n",n);
    }
    else {
        g_ct_pimpl->imp_for(n, f, context);
    }
}
