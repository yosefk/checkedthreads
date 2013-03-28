#include "checkedthreads.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "time.h"

extern "C" int ct_nprocs(); // not a part of the interface but an extern function...

int main() {
    ct_init(0);
    usec_t it_took = usecs([] {
        ctx_for(50, [](int i) {
            if(i==1 || i==2) {
                sleep(1);
                printf("slept at i=%d\n",i);
            }
        });
    });
    if(it_took > 1500000) {
        if(ct_nprocs() == 1) {
            printf("this machine has just one CPU - checkedthreads isn't very useful here...\n");
        }
        else {
            printf("with %d cores, it took more than a second and a half to do two parallel single-second sleeps - something is wrong\n", ct_nprocs());
            exit(1);
        }
    }
    ct_fini();
}
