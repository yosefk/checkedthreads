#include <stdio.h>
#include <stdlib.h>
#include "checkedthreads.h"
#include <unistd.h>

#define N 100
#define SCALE 3

#include "check.h"

int main() {
    int array[N]={0};

    ct_init(0);
    ctx_for(N, [&](int index) {
        array[index] += index*SCALE;
    });
    print_and_check_results(array);
    ct_fini();
    return 0;
}
