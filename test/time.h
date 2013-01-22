#include <sys/time.h>

typedef unsigned long long usec_t;

usec_t curr_usec() {
    struct timeval tv;
    gettimeofday(&tv,0);
    return tv.tv_usec + tv.tv_sec*1000000ULL;
}
