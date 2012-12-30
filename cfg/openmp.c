#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define N 100
#define MAX_THREADS 100

int data_arr[N]={0};
int thread_hist[MAX_THREADS]={0};

void run_omp_for(void) {
    int i;
#pragma omp parallel for
    for(i=0; i<N; ++i) {
        data_arr[i]=i;
        int tn = omp_get_thread_num();
        if(tn >= 0 && tn < MAX_THREADS) {
            thread_hist[tn]++;
        }
    }
}

void test_omp_results() {
    int i;
    for(i=0; i<N; ++i) {
        if(data_arr[i] != i) {
            printf("wrong result at %d\n", i);
            exit(1);
        }
    }
    for(i=0; i<MAX_THREADS; ++i) {
        if(thread_hist[i] != 0) {
            printf("OpenMP thread %d used %d times\n", i, thread_hist[i]);
        }
    }
    printf("OpenMP test passed\n");
}

int main() {
    run_omp_for();
    test_omp_results();
}
