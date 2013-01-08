checkedthreads
==============

checkedthreads: no race condition goes unnoticed! API, auto load balancing, Valgrind-based checking.

What race conditions can be found?
==================================

All of them! checkedthreads provides two verification methods:

* *Fast verification* using a debug scheduler that changes the order of events.
* *Thorough verification* using Valgrind-based instrumentation that monitors memory accesses
  and flags accesses to data owned by another thread.

There are more details below; the upshot is that very race condition that could *ever* manifest on your inputs
will be found.

Why this framework?
===================

checkedthreads is a fork-join parallelism framework not unlike many others, such as Intel TBB,
Microsoft PPL, Cilk, OpenMP or GNU libstdc++ parallel mode. Why would one choose checkedthreads?

Here are some checkedthreads features you may find useful, and which may compell one to use
checkedthreads if a desirable subset of these features is not available in another framework:

* **Guaranteed bug detection**. Concurrent imperative programs have a bad reputation because of
  hard-to-chase bugs. For checkedthreads, easy debugging is a top priority: the API is designed
  to make it possible to find **all** concurrency bugs that could ever manifest on given data,
  and a Valgrind-based checker is provided that does so.
* **Integration with other frameworks**. If your code already uses TBB or OpenMP, you can have
  checkedthreads use TBB or OpenMP to run the tasks you create with the checkedthreads API.
  This way you can use checkedthreads alongside another framework without the two fighting over
  the machine. (Please tell if you'd like to have checkedthreads use another framework such as PPL.)
* **Dynamic load balancing**. checkedthreads comes with its own scheduler where all tasks are
  put in a single queue and processed by the thread from the workers pool which is "the quickest
  to dequeue it". (When using TBB or OpenMP, checkedthreads tries to approximate this scheduling
  policy.) A single queue is not necessarily scalable to a thousand of cores, but it otherwise provides
  optimal load balancing: work gets done as soon as someone is available to do it. So you get nice
  performance on practical hardware configurations.
* **Custom schedulers**: if you prefer a different scheduling policy, you can implement a scheduler
  of your own - you need to implement the same simple interface that is used to implement
  schedulers supplied together with checkedthreads.
* **A C89 as well as a C++11 API**. No compiler extensions (pragmas, keywords, etc.) are involved,
  and while C++11 lambdas and variadic templates are used when available to provide some syntactic
  sugar, the underlying C89 API is useable directly as well.
* **Free** as in no license, no charge, and no restrictions on how the code may be used.
* **Portability**. Very little is assumed about the target platform. It is enough to have a C89
  compiler and an address space shared by a bunch of threads - in fact you don't even need "threads"
  as in "an OS with preemptive scheduling"; you could rather easily port checkedthreads to run
  on a machine without any OS. (However, currently checkedthreads is only developed and tested on
  Linux [Ubuntu 12]; please tell if you have problems using it on another platform or if you
  want it to be easier to build it on another platform.)

Why another framework?
======================

In a nutshell: for computational code, raw threads have two problems - manual load balancing
and race conditions. Automatic load balancing is handled nicely by existing frameworks
such as TBB and OpenMP. AFAIK, automatic detection of race conditions is not. Verification
is not the focus of current parallelism frameworks, flexibility and performance are.

And yet everyone is talking about parallelism bugs, and how hard it is to debug concurrent,
imperative programs; and they're right.

But it doesn't have to be hard. This framework provides APIs for writing *easily testable*
parallel programs - and tools for testing programs written using this API.

When is this framework for me?
==============================

* When your programs are parallelized to gain speed. Currently, checkedthreads is not designed
  to support inherently concurrent programs like those managing banks and websites.
* When you have a serial program that you want to parallelize - or already
  parallelized with OpenMP, TBB or raw threads; you can use checkedthreads alongside any of these.
* When you're worried about race conditions. (This isn't always the case; perhaps the program
  is small enough, or it's mostly serial, or the team is small enough and knows very well
  what's going on. The "frightening" case is when you parallelize hairy, large, stateful programs.)
* When you want to port to a "weird" platform. checkedthreads requires very little of the compiler
  (C89) and of the OS (it doesn't even need context switching and it's easy to port to processors
  with no OS at all). TBB, OpenMP, and even raw threads require more.

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
