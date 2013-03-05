#include "checkedthreads.h"
#include <stdio.h>
#include <stdlib.h>
#define RN 10
#define N (RN*RN)
#define SCALE 1
#include "check.h"

#include <math.h>
volatile double d[N];

int main(int argc, char** argv) {
    int array[N]={0};
    int bug_i = -1;
    int bug_j = -1;
    if(argc == 3) {
        bug_i = atoi(argv[1]);
        bug_j = atoi(argv[2]);
    }

    ct_init(0);
    ctx_for(RN, [&](int i) {
        ctx_for(RN, [&](int j) {
            int ind = i*RN + j;

            array[ind] = ind;

            if(j == bug_j) {
                array[ind+1] = 1;
                if(i == bug_i) {
                    array[ind+RN] = 1;
                }
            }
            d[ind]=sin((double)i*j) + cos((double)i*j);
        });
        if(i == bug_i) {
            array[(i+1)*RN] = 1;
        }
    });
    print_and_check_results(array);
    ct_fini();
    return 0;
}
