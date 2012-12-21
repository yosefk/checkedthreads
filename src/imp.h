#ifndef CT_IMP_H_
#define CT_IMP_H_

#include "checkedthreads.h"

typedef void (*ct_imp_init_func)(int num_threads);
typedef void (*ct_imp_fini_func)(void);
typedef void (*ct_imp_for_func)(int n, ct_ind_func f, void* context);

typedef struct {
    ct_imp_init_func imp_init;
    ct_imp_fini_func imp_fini;
    ct_imp_for_func imp_for;
} ct_imp;

extern int g_ct_verbose;

#endif /* CT_IMP_H_ */
