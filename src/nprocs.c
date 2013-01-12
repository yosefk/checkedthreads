#include "nprocs.h"
#include <unistd.h>

int ct_nprocs() {
    return sysconf( _SC_NPROCESSORS_ONLN );
}
