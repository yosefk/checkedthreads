#include "checkedthreads.h"
#include "time.h"
#include <stdio.h>
#ifdef CT_TBB
#include <tbb/tbb.h>
#endif
#include <numeric>
#include <algorithm>

template<class Iter, class T, class BinOp>
T ctx_accumulate(Iter first, Iter last, T init, BinOp f,
                 int serial_cutoff=1024) {
    const int rec=8;
    int n = last - first;
    int chunk_size = n/rec;
    if(n <= serial_cutoff || chunk_size < 2) {
        return std::accumulate(first, last, init, f);
    }
    else {
        T part[rec];
        ctx_for(rec, [=,&part] (int i) {
            Iter chunk_first = first + chunk_size*i;
            Iter chunk_last = i==rec-1 ? last : chunk_first + chunk_size;
            part[i] = ctx_accumulate(chunk_first+1, chunk_last, *chunk_first, f, serial_cutoff);
        });
        return std::accumulate(part, part+rec, init);
    }
}

#define N (1024*1024*7)

int main() {
    ct_init(0);
    int* arr = new int[N];
    for(int i=0; i<N; ++i) {
        arr[i] = i;
    }
    auto plus = [] (int a,int b)->int { return a+b; };
    usec_t s1 = curr_usec();
    int sum1 = std::accumulate(arr, arr+N, 20, plus);
    usec_t s2 = curr_usec();
    int sum2 = ctx_accumulate(arr, arr+N, 20, plus, 1024*32);
    usec_t s3 = curr_usec();
#ifdef CT_TBB
    int sum3 = tbb::parallel_reduce(tbb::blocked_range<int>(0, N, 1024*32), 0,
            [arr](const tbb::blocked_range<int>& r, int init)->int {
                return std::accumulate(arr+r.begin(), arr+r.end(), init);
            },
            plus) + 20;
#else
    int sum3 = sum2;
#endif
    usec_t s4 = curr_usec();
    printf("sum:\nserial: %d\nparallel: %d\nTBB: %d\n", int(s2-s1), int(s3-s2), int(s4-s3));
    ct_fini();
    if(sum1 != sum2 || sum2 != sum3) {
        printf("error: sums differ\n");
        return 1;
    }
    return 0;
}

