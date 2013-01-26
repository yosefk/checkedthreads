#ifndef CT_IMP_H_
#define CT_IMP_H_

#include "checkedthreads.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ct_canceller {
    volatile int cancelled; /* 1 after ct_cancel is called */
    void* sched_data; /* scheduler-specific */
};

typedef void (*ct_imp_init_func)(const ct_env_var* env);
typedef void (*ct_imp_fini_func)(void);
typedef void (*ct_imp_for_func)(int n, ct_ind_func f, void* context, ct_canceller* c);
/* cancelling functions, as well as the scheduler-specific data in ct_canceller,
   are useful if the underlying framework has a notion of cancellation tokens
   (if it doesn't have such a notion, we simply check the cancelled flag every time
   the framework is about to call user's code). in practice, the one framework
   where this seems to be useful is Microsoft's PPL. */
typedef void (*ct_imp_canceller_init_func)(ct_canceller* c);
typedef void (*ct_imp_canceller_fini_func)(ct_canceller* c);
typedef void (*ct_imp_cancel_func)(ct_canceller* c);

typedef struct {
    const char* name;
    ct_imp_init_func imp_init;
    ct_imp_fini_func imp_fini;
    ct_imp_for_func imp_for;
    ct_imp_canceller_init_func imp_canceller_init; /* may be 0 */
    ct_imp_canceller_fini_func imp_canceller_fini; /* may be 0 */
    ct_imp_cancel_func imp_cancel; /* may be 0 */
} ct_imp;

const char* ct_getenv(const ct_env_var* env, const char* name, const char* default_value);

#ifdef __cplusplus
}
#endif

#endif /* CT_IMP_H_ */
