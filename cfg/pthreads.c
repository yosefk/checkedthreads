#include <stdio.h>
#include <pthread.h>

int set_by_child = 0;

void* child_fn(void* arg) {
    set_by_child = 1;
    return 0;
}

int main() {
    pthread_t child;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&child, &attr, child_fn, 0);
    pthread_join(child, 0);
    if(set_by_child == 1) {
        printf("pthreads test passed\n");
    }
    else {
        printf("set_by_child not set by child!\n");
    }
}
