#include "checkedthreads.h"

void ctx_for_ind_func(int ind, void* context)
{
    const ctx_ind_func& f = *(ctx_ind_func*)context;
    f(ind);
}

void ctx_for(int n, const ctx_ind_func& f)
{
    ct_for(n, ctx_for_ind_func, (void*)&f);
}
