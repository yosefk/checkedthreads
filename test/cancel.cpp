#include "checkedthreads.h"
#include "time.h"
#include <stdlib.h>
#include <stdio.h>

//a really weird recursive find - for testing...
int* find(int* a, int n, int lookfor, ct_canceller* c) {
    if(n<5) {
        for(int i=0; i<n; ++i) {
            if(a[i] == lookfor) {
                if(c) { ct_cancel(c); }
                return a+i;
            }
        }
        return 0;
    }
    int* left=0;
    int* right=0;
    int leftn = n/2;
    int rightn = n-leftn;
    ctx_invoke(
        [&] { left=find(a, leftn, lookfor, c); },
        [&] { right=find(a+leftn, rightn, lookfor, c); },
        c
    );
    if(left) {
        if(c) { ct_cancel(c); }
        return left;
    }
    if(right) {
        if(c) { ct_cancel(c); }
        return right;
    }
    return 0;
}

int main() {
    ct_init(0);
    const int N = 1024*256;
    int* arr = new int[N];
    for(int i=0; i<N; ++i) {
        arr[i] = i;
    }
    int lookfor = 395;
    int pos = 0;
    usec_t no_cancel = usecs([&] {
        ctx_for(N, [&](int i) {
            if(arr[i] == lookfor) {
                pos = i;
            }
        });
    });
    auto checkpos = [&] {
        if(pos != lookfor) {
            printf("error: found %d at %d\n", lookfor, pos);
            exit(1);
        }
    };
    checkpos();
    pos = 0;
    usec_t with_cancel = usecs([&] {
        ct_canceller* c = ct_alloc_canceller();
        ctx_for(N, [&](int i) {
            if(arr[i] == lookfor) {
                pos = i;
                ct_cancel(c);
            }
        }, c);
        ct_free_canceller(c);
    });
    checkpos();

    printf("plain loop\n");
    printf("without cancelling: %d\n", int(no_cancel));
    printf("with cancelling: %d\n", int(with_cancel));

    no_cancel = usecs([&] { pos = find(arr, N, lookfor, 0)-arr; });
    checkpos();

    with_cancel = usecs([&] {
        ct_canceller* c = ct_alloc_canceller();
        pos = find(arr, N, lookfor, c)-arr;
        ct_free_canceller(c);
    });
    checkpos();

    printf("nested loop\n");
    printf("without cancelling: %d\n", int(no_cancel));
    printf("with cancelling: %d\n", int(with_cancel));

    ct_fini();
    return 0;
}
