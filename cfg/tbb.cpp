#include <tbb/tbb.h>
#include <cstdio>

#define N 100

int data_arr[N]={0};

void test_tbb_results() {
    for(int i=0; i<N; ++i) {
        if(data_arr[i] != i) {
            printf("wrong result at %d\n", i);
            exit(1);
        }
    }
    printf("TBB test passed\n");
}

void callback(int i) {
    data_arr[i] = i;
}

int main()
{
    tbb::parallel_for(0, N, callback);
    test_tbb_results();
}
