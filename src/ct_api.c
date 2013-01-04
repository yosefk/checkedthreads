#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "imp.h"

extern ct_imp g_ct_tbb_imp;
extern ct_imp g_ct_serial_imp;
extern ct_imp g_ct_openmp_imp;
extern ct_imp g_ct_shuffle_imp;
extern ct_imp g_ct_valgrind_imp;
extern ct_imp g_ct_pthreads_imp;

ct_imp* g_ct_imps[] = {
    &g_ct_tbb_imp,
    &g_ct_serial_imp,
    &g_ct_openmp_imp,
    &g_ct_shuffle_imp,
    &g_ct_valgrind_imp,
    &g_ct_pthreads_imp,
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
        const char* namei = g_ct_imps[i]->name;
        if(namei && strcmp(namei, name) == 0) {
            return g_ct_imps[i];
        }
        ++i;
    }
    return 0;
}

/* choosing the default scheduler isn't trivial because we don't easily know
   which are available; it depends on config.h and on the library we're linked into. */
const char* ct_default_sched() {
    const char* prefs[] = {"openmp","tbb","pthreads",0};
    int j=0;
    while(prefs[j]) {
        int i=0;
        while(g_ct_imps[i]) {
            const char* namei = g_ct_imps[i]->name;
            if(namei && strcmp(namei,prefs[j]) == 0) {
                return namei;
            }
            ++i;
        }
        ++j;
    }
    return "serial"; /* if no parallel scheduler is available, is a serial one better than crashing?.. */
}

void ct_init(const ct_env_var* env) {
    int num_threads = atoi(ct_getenv(env, "CT_THREADS", "0"));
    const char* default_sched = ct_default_sched();
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

    if(g_ct_verbose) {
        printf("checkedthreads: initialized\n");
    }
}

void ct_fini(void) {
    g_ct_pimpl->imp_fini();
    g_ct_pimpl = 0;
    if(g_ct_verbose) {
        printf("checkedthreads: finalized\n");
    }
}

void ct_dispatch_task(int index, void* context)
{
    const ct_task* tasks = (const ct_task*)context;
    const ct_task* t = tasks + index;
    t->func(t->arg);
}

void ct_invoke(const ct_task tasks[])
{
    int i;
    for(i=0; tasks[i].func; ++i);
    ct_for(i, ct_dispatch_task, (void*)tasks);
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
