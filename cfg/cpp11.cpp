#include <cstdio>

int set_by_lambda = 0;

template<class F>
void call(const F& f) { f(); }

int main() {
    call([] { set_by_lambda=1; });
    if(set_by_lambda) {
        printf("C++11 test passed\n");
    }
    else {
        printf("set_by_lambda not set by lambda!\n");
    }
}
