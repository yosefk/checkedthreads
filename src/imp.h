#ifndef CT_IMP_H_
#define CT_IMP_H_

#include "checkedthreads.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ct_imp_init_func)(const ct_env_var* env);
typedef void (*ct_imp_fini_func)(void);
typedef void (*ct_imp_for_func)(int n, ct_ind_func f, void* context);

typedef struct {
    const char* name;
    ct_imp_init_func imp_init;
    ct_imp_fini_func imp_fini;
    ct_imp_for_func imp_for;
} ct_imp;

const char* ct_getenv(const ct_env_var* env, const char* name, const char* default_value);

#ifdef __cplusplus
}
#endif

#endif /* CT_IMP_H_ */
