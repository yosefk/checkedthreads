#include <stdlib.h>
#include "imp.h"

/* TODO: add a seed */
/* TODO: use local state instead of rand()'s global state. */

int g_ct_random_reverse = 0;

/* based on GNU std::random_shuffle */
void ct_random_shuffle(int* p, int n) {
    int i;
    for(i=1; i<n; ++i) {
        /* swap p[i] with a random element in [0,i] */
        int j = rand() % (i+1);
        int tmp = p[j];
        p[j] = p[i];
        p[i] = tmp;
    }
}

int* ct_rand_perm(int n) {
    int* p = (int*)malloc(n*sizeof(int));
    int i;
    for(i=0; i<n; ++i) {
        p[i] = i;
    }
    ct_random_shuffle(p, n);
    if(g_ct_random_reverse) {
        for(i=0; i<n/2; ++i) {
            int tmp = p[i];
            p[i] = p[n-i-1];
            p[n-i-1] = tmp;
        }
    }
    return p;
}

void ct_shuffle_init(const ct_env_var* env) {
    /* TODO: do not use rand()! */
    srand(atoi(ct_getenv(env, "CT_RAND_SEED", "12345")));
    g_ct_random_reverse = atoi(ct_getenv(env, "CT_RAND_REV", "0"));
}

void ct_shuffle_fini(void) {
}

void ct_shuffle_for(int n, ct_ind_func f, void* context, ct_canceller* c) {
    int* perm = ct_rand_perm(n);
    int i;
    for(i=0; i<n; ++i) {
        if(c->cancelled) {
            break;
        }
        f(perm[i], context);
    }
    free(perm);
}

ct_imp g_ct_shuffle_imp = {
    "shuffle",
    &ct_shuffle_init,
    &ct_shuffle_fini,
    &ct_shuffle_for,
    0, 0, 0, /* cancelling functions */
};
