#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "imp.h"

#define STORED_MAGIC 0x12345678
#define CONST_MAGIC "Valgrind command"
#define MAX_CMD 128

struct {
    volatile int32_t stored_magic;
    const char const_magic[16];
    volatile char payload[MAX_CMD];
} g_ct_valgrind_cmd = { 0, CONST_MAGIC, "" };

void ct_valgrind_ptr(int oft, volatile const void* ptr) {
    *(volatile const void**)&g_ct_valgrind_cmd.payload[oft] = ptr;
}

void ct_valgrind_int(int oft, int num) {
    *(volatile int32_t*)&g_ct_valgrind_cmd.payload[oft] = num;
}

void ct_valgrind_cmd(const char* str) {
    int i;
    for(i=0; str[i]; ++i) {
        g_ct_valgrind_cmd.payload[i] = str[i];
    }
    g_ct_valgrind_cmd.stored_magic = STORED_MAGIC;
}

void ct_shuffle_init(const ct_env_var* env);
void ct_valgrind_init(const ct_env_var* env) {
    ct_shuffle_init(env); /* pass $CT_RAND_SEED and $CT_RAND_REV */
}

void ct_valgrind_fini(void) {
}

int* ct_rand_perm(int n);

void ct_valgrind_for_loop(int n, ct_ind_func f, void* context, ct_canceller* c) {
    int i;
    /* int* perm = ct_rand_perm(n); FIXME!! */

    for(i=0; i<n; ++i) {
        int ind = i; /* FIXME!! perm[i]; */
        /* thread ID != index because of thread-local storage
           [and because of the ID range being smaller... but that's another matter.] */
        ct_valgrind_int(4, ind%255); /* there are 255 IDs (0 is reserved;
                                      1 is added by Valgrind and subtracted back in messages). */
        ct_valgrind_cmd("thrd");

        ct_valgrind_int(4, ind);
        ct_valgrind_cmd("iter");

        f(ind, context);

        ct_valgrind_int(4, ind);
        ct_valgrind_cmd("done");
    }
    /* free(perm); FIXME!! */
}

/* "volatile" for portable inlining prevention (instead of __attribute__((noinline))) */
volatile ct_imp_for_func g_ct_valgrind_for = &ct_valgrind_for_loop;

/* under Valgrind, loops run to completion even if canceled */
void ct_valgrind_for(int n, ct_ind_func f, void* context, ct_canceller* c) {
    volatile int local=0;
    ct_valgrind_cmd("begin_for");

    ct_valgrind_ptr(8, &local);
    ct_valgrind_cmd("stackbot");

    (*g_ct_valgrind_for)(n,f,context,c);

    ct_valgrind_cmd("end_for");
}

int ct_debug_get_owner(const void* addr) {
    volatile int i,m;
    ct_valgrind_ptr(8, addr);
    ct_valgrind_cmd("getowner");
    for(i=0; i<5; ++i) {
        m = g_ct_valgrind_cmd.stored_magic;
    }
    return g_ct_valgrind_cmd.stored_magic == STORED_MAGIC ? CT_OWNER_UNKNOWN : g_ct_valgrind_cmd.stored_magic;
}

ct_imp g_ct_valgrind_imp = {
    "valgrind",
    &ct_valgrind_init,
    &ct_valgrind_fini,
    &ct_valgrind_for,
    0, 0, 0, /* cancelling functions (TODO: some should be non-0) */
};
