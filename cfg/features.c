/* this isn't really C code - it's a file used by build.py to figure out
   what features are enabled in checkedthreads_config.h (it does so by
   running cpp and checking which features appear in the output) */
#include "checkedthreads_config.h"

#ifdef CT_CXX11
C++11 enabled
#endif

#ifdef CT_OPENMP
OpenMP enabled
#endif

#ifdef CT_TBB
TBB enabled
#endif

#ifdef CT_PTHREADS
pthreads enabled
#endif
