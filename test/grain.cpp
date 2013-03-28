#include "checkedthreads.h"
#include "time.h"
#include <stdlib.h>
#include <stdio.h>

#define N (1024*1024*64)

int main(int argc, char** argv) {
    int grain = 100;
    if(argc>1) {
        grain = atoi(argv[1]);
    }
    ct_init(0);
    int* arr = new int[N];
    for(int i=0; i<N; ++i) {
        arr[i] = i;
    }
    usec_t t1 = curr_usec();
    ctx_for(N/grain, [=](int i) {
        int start = i*grain;
        int finish = start + grain;
        for(int k=start; k<finish; ++k) {
            arr[i] *= 2;
        }
    });
    usec_t t2 = curr_usec();
    printf("time: %d\n", int(t2-t1));
    ct_fini();
}
