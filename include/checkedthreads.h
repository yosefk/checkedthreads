#ifndef CHECKEDTHREADS_H_
#define CHECKEDTHREADS_H_

#include "checkedthreads_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* initialization */
typedef struct {
    const char* name;
    const char* value;
} ct_env_var;

/* env may be 0, in which case environment variables are obtained using getenv.
   if env is not 0, then {0,0} is the sentinel; environment variables not
   specified in env are still looked up using getenv.

   environment variables:

   $CT_SCHED: serial, shuffle, valgrind, openmp, tbb, pthreads.
   $CT_THREADS: number of threads, including main; "0" means "a thread per core".
   $CT_VERBOSE: 2(print indexes), 1(print loops), 0(silent-default).
   $CT_RAND_SEED: seed for schedulers randomizing order (shuffle & valgrind).
   $CT_RAND_REV: reverse each random index sequence yielded by the given seed.

   note that the parallel schedulers such as openmp and tbb currently
   specify two things which are conceptually separate: the "threading platform"
   (do we access threading using OpenMP or TBB interfaces?) and the scheduling
   policy (do we partition indexes statically or dynamically?).

   if we wish to support multiple policies in the future, we'll add a $CT_POLICY
   or some such; currently it seems undesirable in the sense that changing
   the policy is unlikely to improve the performance of programs written
   by people who understood, and counted on, another policy.
 */
void ct_init(const ct_env_var* env);
void ct_fini(void);

/* cancelling: if ct_for/ct_invoke is passed a canceller, then
   ct_cancel() can be used to cancel that for/invoke. a single
   canceller can be passed to many fors/invokes. */
typedef struct ct_canceller ct_canceller;

ct_canceller* ct_alloc_canceller(void);
void ct_free_canceller(ct_canceller* c);
void ct_cancel(ct_canceller* c);
int ct_cancelled(ct_canceller* c);

/* invoke a set of tasks - make N async function calls */
typedef void (*ct_task_func)(void* arg);
typedef struct {
    ct_task_func func;
    void* arg;
} ct_task;
/* a task with func==0 is the sentinel */
void ct_invoke(const ct_task tasks[], ct_canceller* c);

/* N async function calls f(0) ... f(n-1) */
typedef void (*ct_ind_func)(int ind, void* context);
void ct_for(int n, ct_ind_func f, void* context, ct_canceller* c);

/* under Valgrind or other ownership-tracking environment,
   returns an ID of the owner of the given address; elsewhere,
   always returns CT_OWNER_UNKNOWN */
#define CT_OWNER_COMMON (-1) /* everybody can access it */
#define CT_OWNER_UNKNOWN (-2) /* not under Valgrind or equivalent */
int ct_debug_get_owner(const void* addr);

#ifdef __cplusplus
} /* extern "C" */

/* we could check the value of __cplusplus but not all compilers implement it correctly */
#ifdef CT_CXX11

#include <functional>
typedef std::function<void(int)> ctx_ind_func;

void ctx_for(int n, const ctx_ind_func& f, ct_canceller* c=0);

typedef std::function<void(void)> ctx_task_func;
struct ctx_task_node_ {
    ctx_task_func* func;
    ctx_task_node_* next;
};
void ctx_invoke_(ctx_task_node_* head, ct_canceller* c=0);
template<typename First, typename... Rest>
void ctx_invoke_(ctx_task_node_* head, const First& first, Rest... rest) {
    ctx_task_func func(first);
    ctx_task_node_ new_head = { &func, head };
    ctx_invoke_(&new_head, rest...);
}
/* ...and ctx_invoke itself. */
template<typename First, typename... Rest>
void ctx_invoke(const First& first, Rest... rest) {
    ctx_task_func func(first);
    ctx_task_node_ head = { &func, 0 };
    ctx_invoke_(&head, rest...);
}

#endif /* CT_CXX11 */

#endif /* __cplusplus */

#endif /* CHECKEDTHREADS_H_ */
