checkedthreads
==============

checkedthreads: concurrent, imperative, verified. APIs, a load-balancing scheduler, and a Valgrind-based checker

API
===

typedef void (*ct_index_func)(int index, void* context);

void ct_for_each(int n, ct_index_func f, void* context);

typedef std::function<void(int)> ctx_index_func;

void ctx_for_each(int n, const ctx_index_func& f);

* Reductions are somewhat low-priority:
  If you have a modest amount of cores (4-8), then reductions use an even smaller number of cores. Also, many reductions
  can be handled using another foreach: for instance, every thread computes a histogram of a subset of the data,
  then another foreach is used to sum a different subrange of the histogram.
* In the histogram example, we can have per-thread histograms accessible past the end of foreach, which means
  we need off-stack thread-local storage; and we may want to have a concurrent histogram instead. The latter
  is easy to do with an atomic_add (easier than a concurrent hash table...), but not trivial to verify - we
  need a way to make sure that the side effects are commutative, and that the data structure isn't read in the
  same loop that modifies it.
