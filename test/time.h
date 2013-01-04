#include <sys/time.h>

unsigned long long curr_usec() {
    struct timeval tv;
    gettimeofday(&tv,0);
    return tv.tv_usec + tv.tv_sec*1000000ULL;
}
