checkedthreads
==============

checkedthreads: parallel, simple, safe. APIs, a load-balancing scheduler, and a Valgrind-based checker

API
===

```C++
/* C: ct_for(10, &callback, &args); */
typedef void (*ct_ind_func)(int ind, void* context);
void ct_for(int n, ct_ind_func f, void* context);

//C++: ctx_for(10, [=] (int i) { use(i, args); });
typedef std::function<void(int)> ctx_ind_func;
void ctx_for(int n, const ctx_ind_func& f);
```

* Reductions are somewhat low-priority:
  If you have a modest amount of cores (4-8), then reductions use an even smaller number of cores. Also, many reductions
  can be handled using another foreach: for instance, every thread computes a histogram of a subset of the data,
  then another foreach is used to sum a different subrange of the histogram.
* In the histogram example, we can have per-thread histograms accessible past the end of foreach, which means
  we need off-stack thread-local storage; and we may want to have a concurrent histogram instead. The latter
  is easy to do with an atomic_add (easier than a concurrent hash table...), but not trivial to verify - we
  need a way to make sure that the side effects are commutative, and that the data structure isn't read in the
  same loop that modifies it.

Thread-safe data structures
===========================

* Commutativity is not always possible to verify (histograms are always OK, but not unordered lists where peope count
  on the order). This is why we have a random shuffling scheduler.
* All data structures have a state: RO, WO, RW. If they are RW, then they bomb when used from threads. If they are RO/WO,
  they bomb when they're written/read.
* The valgrind checker is simply told to ignore accesses to thread-safe data structures, thus not even attempting
  to verify if they're actually thread-safe.
  
Task graphs, pipelines, etc.
============================

Parallel for scales and load-balances nicely, it's easy to spell and easy to check. Task graphs and pipelines
do not scale that well (a 5-stage pipeline has no use for 8 cores), they don't load-balance that well (the
heaviest task/pipeline stage limits throughput), and they aren't that easy to spell (especially task graphs
where "graph" is the shape of dependencies). If you're on symmetrical hardware and you're trying to speed up
a computational process with no I/O, then it's hard to see how parallel fors aren't good enough.
(Task graphs can be better in cache utilization if each task have a lot of cached data and each task uses
the cache fully where a data-parallel system would end up replicating data in many L1 caches. But it still
doesn't scale and doesn't load-balance.) 

Automatic parallelization
=========================

...Is nice, because it guarantees correctness. However, shuffling and instrumentation are very close to
guaranteed correctness, and automatic parallelization is impossible for any popular imperative language
and furthermore, it can never "fully relieve programmers of parallelism burdens" because iterative algorithms
must still be manually converted to non-iterative, parallelizable algorithms. A framework with correctness
achieved using dynamic verification in an existing popular language is not necessarily more expensive to
adopt than a compiler staticallly guaranteeing correctness that requires to switch to a different
language or to use a restricted subset of your current language in much of your code.

Data-dependent termination
==========================

As in, while instead of for; worth looking into.

Coding style
============

* Everything prefixed with ct_ (ctx_ for C++ identifiers), except for struct members.
* Globals prefixed with g_ct/g_ctx; no static variables (all are extern).
* Indentation: 1TBS, 4 spaces per level, no hard tabs.
* Everything is lowercase, underscore_separated.
