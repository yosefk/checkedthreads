checkedthreads
==============

checkedthreads is a fork-join parallelism framework providing:

* **A simple API** - nested, cancellable parallel loops and function calls.
* **Automatic load balancing** across the available cores.
* **Automated race detection** using debugging schedulers and Valgrind-based instrumentation.

If you have dozens of developers working on millions of lines of
multithreaded code, checkedthreads is supposed to let you ship new versions **without worrying about parallelism bugs**.
It's based on a decade of experience in that kind of environment.

What race conditions will be found?
===================================

All of them! checkedthreads provides two verification methods:

* **Fast verification** using a debugging scheduler that changes the order of events.
* **Thorough verification** using Valgrind-based instrumentation that monitors memory accesses
  and flags accesses to data owned by another thread.

There are more details below; the upshot is that every race condition will be found if:

* It could **ever** manifest on the given inputs.
* The bug is actually a race condition :-) (What looks like a race condition but isn't a
  race condition? Consider using uninitialized memory returned
  by malloc. This is a bug regardless of parallelism. This also leads to non-deterministic
  results in parallel programs. But the bug is not a race condition, and while
  checkedthreads may help find the bug, no guarantees are made - unlike with pure race conditions.)

Nice features
=============

checkedthreads is a fork-join framework not unlike many others, such as Intel TBB,
Microsoft PPL, Cilk, OpenMP or GNU libstdc++ parallel mode. How to choose a framework?

Here are some nice features of checkedthreads; you can compare other frameworks' features
to this list as you shop around:

* Pretty much **guaranteed** bug detection
* Integration with other frameworks
* Dynamic load balancing
* Custom schedulers
* A C89 and a C++11 API
* "Free" as in "do whatever you want with it"
* Easily portable (at least in theory)

Details:

* **Guaranteed bug detection**. Concurrent imperative programs have a bad reputation because of
  hard-to-chase bugs. For checkedthreads, **easy debugging is a top priority**: the API is designed
  to make it possible to find **all** concurrency bugs that could ever manifest on given data,
  and a Valgrind-based checker is provided that does so.
* **Integration with other frameworks**. If your code already uses TBB or OpenMP, you can have
  checkedthreads rely on TBB or OpenMP to run the tasks you create with the checkedthreads API.
  This way, you can use checkedthreads alongside another framework without the two fighting over
  the machine. (Please tell if you'd like to use checkedthreads alongside another framework such as PPL.)
* **Dynamic load balancing**. checkedthreads comes with its own scheduler where all tasks are
  put in a single queue and processed by the thread from the workers pool which is "the quickest
  to dequeue it". (When using TBB or OpenMP, checkedthreads tries to approximate this scheduling
  policy.) A single queue is not necessarily scalable to 1000 cores, but it otherwise provides
  optimal load balancing: work gets done as soon as someone is available to do it. So you get nice
  performance on practical hardware configurations.
* **Custom schedulers**: if you prefer a different scheduling policy, you can implement a scheduler
  of your own - you need to implement the same simple interface that is used to implement
  schedulers supplied together with checkedthreads.
* **A C89 as well as a C++11 API**. No compiler extensions (pragmas, keywords, etc.) are involved,
  and while C++11 lambdas and variadic templates are used when available to provide some syntactic
  sugar, the underlying C89 API is useable directly as well.
* **Free** as in no license, no charge, and no restrictions on how the code may be used. Also
  no warranty of course.
* **Portability**. Very little is assumed about the target platform. It is enough to have a C89
  compiler and an address space shared by a bunch of threads - in fact you don't even need "threads"
  as in "an OS with preemptive scheduling"; you could rather easily port checkedthreads to run
  on a machine without any OS. (However, currently checkedthreads is only developed and tested on
  Linux [Ubuntu 12]; please tell if you have problems using it on another platform or if you
  want it to be easier to build it on another platform.)

Another nice feature, currently, is simplicity and small size, but these were known to
transform into complexity and large size in the past; an effort will be made to avoid that...

There are also missing features - please tell if a feature you need is missing. Making a list
of everything *not* there is a tad hard... one biggie, currently, is concurrency - no means
are provided to wait for events except for issuing a blocking call (which "steals" a thread from
the underlying thread pool, so it's not any good, really). Generally concurrency
is not currently a use case: checkedthreads
is a framework for *parallelizing computational code* which does not interact with the external world.

API
===

In a nutshell:

* You can parallelize **loops** (with ct_for) and **function calls** (with ct_invoke).
* Both parallel loops and function calls can be **nested**.
* A parallel loop or a set of parallel function calls can be **cancelled** before they complete.

Examples using the C++11 API:

```C++
ctx_for(100, [&](int i) {
    ctx_for(100, [&](int j) {
        array[i][j] = i*j;
    });
});
```

Absolutely boneheaded code, but you get the idea. i and j go from 0 to 99; currently there's no way to specify
a start other than 0 or an increment other than 1. There's also no way to control "grain size" -
each index is a separately scheduled task, so it should do a non-trivial amount of work, or the scheduling
overhead will dwarf any gains from running on several cores.

For a better example, here's parallel sorting:
```C++
template<class T>
void quicksort(T* beg, T* end) {
    if (end-beg >= MIN_PAR) {
        int piv = *beg, l = 1, r = end-beg;
        while (l < r) {
            if (beg[l] <= piv) 
                l++;
            else
                std::swap(beg[l], beg[--r]);
        }   
        std::swap(beg[--l], beg[0]);
        //sort the two parts in parallel:
        ctx_invoke( 
            [=] { quicksort(beg, beg+l); },
            [=] { quicksort(beg+r, end); }
        );  
    }
    else {
        std::sort(beg, end);
    }
}
```
A C89 example:

```C
void set_elem(int i, void* context) {
    int* array = (int*)context;
    array[i] = i;
}

void example(void) {
    int array[100];
    ct_for(100, set_elem, array, 0);
}
```
That last "0" is a (null) pointer to a *canceller*; we'll get to that in a moment. Meanwhile, parallel invoke in C89:

```C
void a(void* context) { *(int*)context = 1; }
void b(void* context) { *(int*)context = 2; }

void example(void) {
    int first, second;
    ct_task tasks[] = {
        {a, &first},
        {b, &second},
        {0, 0}
    };
    ct_invoke(tasks, 0);
}
```
Again the last 0 is the canceller.

Planned features
================

Planned features not yet avaialable:

* Proper checking of cancelling (cancelling is only OK if at most one thread writes things)
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
