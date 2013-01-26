#include <sys/time.h>

typedef unsigned long long usec_t;

usec_t curr_usec() {
    struct timeval tv;
    gettimeofday(&tv,0);
    return tv.tv_usec + tv.tv_sec*1000000ULL;
}

#ifdef __cplusplus
template<class F>
usec_t usecs(const F& f) {
    usec_t start = curr_usec();
    f();
    return curr_usec() - start;
}
#endif
