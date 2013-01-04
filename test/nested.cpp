#include "checkedthreads.h"
#include <stdio.h>
#include <stdlib.h>
#define RN 10
#define N (RN*RN)
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
        if(array[i] != i) {
            printf("error at %d!\n", i);
            exit(1);
        }
    }
}
int main() {
    int array[N]={0};

    ct_init(0);
    ctx_for(RN, [&](int i) {
        ctx_for(RN, [&](int j) {
            int ind = i*RN + j;
            array[ind] = ind;
        });
    });
    print_and_check_results(array);
    ct_fini();
    return 0;
}
