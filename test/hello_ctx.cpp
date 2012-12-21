#include <stdio.h>
#include <stdlib.h>
#include "checkedthreads.h"

#define N 100
#define SCALE 3

void print_and_check_results(int array[]) {
    int i;
    printf("results:");
    for(i=0; i<3; ++i) {
        printf(" %d", array[i]);
    }
    printf(" ...");
    for(i=N-3; i<N; ++i) {
        printf(" %d", array[i]);
    }
    printf("\n");
    for(i=0; i<N; ++i) {
        if(array[i] != i*SCALE) {
            printf("error at %d!\n", i);
            exit(1);
        }
    }
}

int main() {
    int array[N]={0};

    ct_init(0);
    ctx_for(N, [&](int index) {
        array[index] = index*SCALE;
        if(index==55) {
            array[index+1] = index*SCALE;
        }
    });
    print_and_check_results(array);
    ct_fini();
    return 0;
}
