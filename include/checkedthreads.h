#ifndef CHECKEDTHREADS_H_
#define CHECKEDTHREADS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* initialization */
typedef struct {
    const char* name;
    const char* value;
} ct_env_var;

/* TODO: num_threads==0 should auto-detect #cores and create a thread per core.
   env may be 0, in which case environment variables are obtained using getenv.
   if env is not 0, then {0,0} is the sentinel.

   environment variables:

   $CT_SCHED: serial, shuffle, balance(default).
   $CT_VERBOSE: 2(print indexes), 1(print loops), 0(silent-default).
 */
void ct_init(int num_threads, const ct_env_var* env);
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
