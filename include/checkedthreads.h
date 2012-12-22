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
   if env is not 0, then {0,0} is the sentinel.

   environment variables:

   $CT_SCHED: serial, shuffle, valgrind, openmp(default), tbb.
   $CT_THREADS: number of threads, including main (TODO: 0 should auto-config.)
   $CT_VERBOSE: 2(print indexes), 1(print loops), 0(silent-default).

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

typedef void (*ct_ind_func)(int ind, void* context);
void ct_for(int n, ct_ind_func f, void* context);

#ifdef __cplusplus
} /* extern "C" */

/* we could check the value of __cplusplus but not all compilers implement it correctly */
#ifdef CT_CXX11

#include <functional>

typedef std::function<void(int)> ctx_ind_func;
void ctx_for(int n, const ctx_ind_func& f);

#endif /* CT_CXX11 */

#endif /* __cplusplus */

#endif /* CHECKEDTHREADS_H_ */
