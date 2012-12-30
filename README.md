checkedthreads
==============

checkedthreads: parallel, simple, safe. APIs, a load-balancing scheduler, and a Valgrind-based checker

Nice things
===========

* Verification using debug schedulers (fast) and Valgrind-based instrumentation (thorough).
  Find every bug that could *ever* manifest on the given inputs!
* *Integrates* with OpenMP, TBB and pthreads ("integrates" is better than than just
  "can run on top of" - not only that, but it works nicely alongside code
  using OpenMP and TBB directly; for example, alongside GNU's parallel STL.)
* Easy to port: no compiler extensions, small C89 code (the tiny C++11 bit is optional),
  very few required from the underlying platform (not even context switching).

Theoretically, another nice thing is simplicity and small size of interface
and implementation. However, time is known to cure both. What *should* remain true
is safety: only such features should be added with which is remains possible
to thoroughly verify programs.

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

Races will be flagged; not necessarily bugs that are /also/ races
=================================================================

Some bugs are *also* race conditions. For instance, mallocing and accessing uninitialized memory
will lead to different results depending on timing; it's also a bug in a serial program. Or,
out of bounds accesses can yield different results depending on timing; they're also bugs
in a serial program. Or, access to on-stack data after the function that allocated it returns, etc.

These bugs will not necessarily be flagged as races by the Valgrind tool. What will be certainly be flagged
as a race is something that has a chance to not be a bug in a serial program but also has a chance
to be a race condition in its parallel version. For instance, access to initialized data through
"legitimatly" obtained pointers will certainly be flagged as a race if there's a chance for it
to be a race.

TODO
====

PIC code support and a libcheckedthreads.so, possibly. Even "without shared libraries",
we need to handle the .got.plt business because as long as you don't use -static, which
you can't if you want valgrind to work, you're going to have these in standard libraries.

currently missing - thread-local storage and some lock-free primitives
(a histogram, an unordered list). another big thing is automatic configuration
and testing.

Coding style
============

* Everything prefixed with ct_ (ctx_ for C++ identifiers), except for struct members.
* Globals prefixed with g_ct/g_ctx; no static variables (all are extern).
* Indentation: 1TBS, 4 spaces per level, no hard tabs.
* Everything is lowercase, underscore_separated. Macros mostly UPPERCASE.
* Valgrind tool code (at valgrind/) should use Valgrind style.
* Style isn't that important.
