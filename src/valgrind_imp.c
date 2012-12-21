#include "imp.h"
#include <stdio.h>
#include <stdint.h>

#define STORED_MAGIC 0x12345678
#define CONST_MAGIC "Valgrind command"
#define MAX_CMD 128

struct {
    volatile uint32_t stored_magic;
    const char const_magic[16];
    volatile char payload[MAX_CMD];
} g_ct_valgrind_cmd = { 0, CONST_MAGIC, "" };

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

void ct_valgrind_init(int num_threads) {
    (void)num_threads;
}

void ct_valgrind_fini(void) {
}

void ct_valgrind_for(int n, ct_ind_func f, void* context) {
    int i;
    ct_valgrind_cmd("begin_for");

    /* FIXME: we ought to have index randomization for proper coverage */
    for(i=0; i<n; ++i) {
        ct_valgrind_int(4, i);
        ct_valgrind_cmd("iter");

        f(i, context);

        ct_valgrind_int(4, i);
        ct_valgrind_cmd("done");
    }
    ct_valgrind_cmd("end_for");
}

ct_imp g_ct_valgrind_imp = {
    "valgrind",
    &ct_valgrind_init,
    &ct_valgrind_fini,
    &ct_valgrind_for,
};
