checkedthreads
==============

checkedthreads: no race condition goes unnoticed! API, auto load balancing, Valgrind-based checking.

What race conditions can be found?
==================================

All of them! checkedthreads provides two verification methods:

* **Fast verification** using a debugging scheduler that changes the order of events.
* **Thorough verification** using Valgrind-based instrumentation that monitors memory accesses
  and flags accesses to data owned by another thread.

There are more details below; the upshot is that every race condition will be found if:

* It could **ever** manifest on the given inputs.
* The bug is actually a race condition :-) (What looks like a race condition but isn't a
  race condition? Consider using uninitialized memory returned
  by malloc. This is a bug regardless of concurrency. This also leads to non-deterministic
  results in concurrent programs. But the bug is not a race condition, and while
  checkedthreads may help find the bug, no guarantees are made - unlike with pure race conditions.)

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

Planned features
================

Planned features not yet avaialable:

* Custom allocator interface (to tell the checker when memory is allocated/freed)
* A compiler (LLVM/gcc) pass in addition to the dynamic Valgrind instrumentation
* A Windows build and integration with PPL

Coding style
============

* Everything prefixed with ct_ (ctx_ for C++ identifiers), except for struct members.
* Globals prefixed with g_ct/g_ctx; no static variables (all are extern).
* Indentation: 1TBS, 4 spaces per level, no hard tabs.
* Everything is lowercase, underscore_separated. Macros mostly UPPERCASE.
* Valgrind tool code (at valgrind/) should use Valgrind style.
* Style isn't that important.
