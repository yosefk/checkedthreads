checkedthreads
==============

checkedthreads is a fork-join parallelism framework for C and C++ providing:

* **Automated race detection** using debugging schedulers and Valgrind-based instrumentation.
* **Automatic load balancing** across the available cores.
* **A simple API** - nested, cancellable parallel loops and function calls.

If you have dozens of developers working on millions of lines of
multithreaded code, checkedthreads will let you painlessly parallelize the code -
and then continuously ship new versions **without worrying about parallelism bugs**.
It's based on a decade of experience in that kind of environment.

Contents
========

* [What race conditions will be found?](#what-race-conditions-will-be-found)
* [Nice features](#nice-features)
* [API](#api)
* [Environment variables](#environment-variables)
* [How race detection works](#how-race-detection-works)
* [Building and installing](#building-and-installing)
* [Planned features](#planned-features)
* [Coding style](#coding-style)
* [Support/contact](#supportcontact)

What race conditions will be found?
===================================

All of them! checkedthreads provides two verification methods:

* **Fast verification** using a debugging scheduler that changes the order of events.
* **Thorough verification** using Valgrind-based instrumentation that monitors memory accesses
  and flags accesses to data owned by another thread.

There are more details [below](#how-race-detection-works); the upshot is that every race condition will be found if:

* It could **ever** manifest on the given inputs.
* The bug is actually a race condition :-) (What looks like a race condition but isn't a
  race condition? Consider using uninitialized memory returned
  by malloc. This is a bug regardless of parallelism. This also leads to non-deterministic
  results in parallel programs. But the bug is not a race condition - it's a memory initialization problem.
  So while checkedthreads may help find the bug, no guarantees are made - unlike with pure race conditions.)

Nice features
=============

checkedthreads is a fork-join framework not unlike many others, such as [Intel TBB](http://en.wikipedia.org/wiki/Intel_Threading_Building_Blocks),
[Microsoft PPL](http://en.wikipedia.org/wiki/Parallel_Patterns_Library),
[Cilk](http://en.wikipedia.org/wiki/Cilk),
[OpenMP](http://en.wikipedia.org/wiki/Openmp) or
[GNU libstdc++ parallel mode](http://gcc.gnu.org/onlinedocs/libstdc++/manual/parallel_mode.html).
How to choose a framework?

Here are some nice features of checkedthreads; you can compare other frameworks' features
to this list as you shop around:

* Pretty much **guaranteed bug detection**
* Integration with other frameworks
* Dynamic load balancing
* A **C89** and a **C++11** API
* "Free" as in "do whatever you want with it"
* Easily portable (at least in theory)
* Custom schedulers

Details:

* **Guaranteed bug detection** is a rare promise in the world of parallel imperative programs,
  which have a bad reputation because of hard-to-chase bugs. The API is designed with the goal
  of making it possible to find **all** parallelism bugs that could manifest on given inputs,
  and tools are provided for doing so.
* **Integration with other frameworks**. If your code already uses TBB or OpenMP, you can have
  checkedthreads rely on TBB or OpenMP to run the tasks you create with the checkedthreads API.
  This way, you can use checkedthreads alongside another framework without the two fighting over
  the machine. (Please tell if you'd like to use checkedthreads alongside another framework such as PPL.)
* **Dynamic load balancing**. checkedthreads comes with its own scheduler where all tasks are
  put in a single queue and processed by the worker thread which is "the quickest
  to dequeue it". (When using TBB or OpenMP, checkedthreads tries to approximate this scheduling
  policy.) A single queue is not necessarily scalable to 1000 cores, but it otherwise provides
  optimal load balancing: work gets done as soon as someone is available to do it. The upshot is that
  you get nice performance on practical hardware configurations.
* **A C89 as well as a C++11 API**. No compiler extensions (#pragmas, keywords, etc.) are involved.
  While C++11 lambdas and variadic templates are used to provide some syntactic
  sugar, the underlying C89 API is useable directly as well.
* **Free** as in "licensed under the FreeBSD license" (the 2-clause thing, see LICENSE.txt).
  I'll gladly license the code under any other license if you want - I originally just said "do whatever you
  want with the code", but people worried that lawyers would much prefer a standard license.
* **Portability**. Very little is assumed about the target platform. It is enough to have a C89
  compiler and an address space shared by a bunch of threads. In fact, you don't even need "threads"
  as in "an OS with preemptive scheduling"; you could rather easily port checkedthreads to run
  on a machine without any OS. (However, currently checkedthreads is only developed and tested on
  Linux [Ubuntu 12]. Please tell if you have problems using it on another platform, or if you
  want it to be easier to build on another platform.)
* **Custom schedulers**: if you prefer a different scheduling policy, you can implement a scheduler
  of your own - you need to implement the same 3-function interface that is used to implement
  the schedulers supplied together with checkedthreads.

Another nice feature, at the moment, is simplicity and small size. However, these were known to
transform into complexity and large size in the past. An effort will be made to avoid that.

There are also missing features - please tell if a feature you need is missing. Making a list
of everything *not* there is a tad hard... One biggie, currently, is concurrency. No means
are provided to wait for events except for issuing a blocking call (which "steals" a thread from
the underlying thread pool, so it's not any good, really). Generally concurrency
is not currently a use case: checkedthreads
is a framework for *parallelizing computational code* which does not interact much with the external world.

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

Absolutely boneheaded code, but you get the idea. i and j go from 0 to 99. Currently there's no way to specify
a start other than 0 or an increment other than 1. There's also no way to control "grain size" -
**each index is a separately scheduled task**. So a non-trivial amount of work should be done per index,
or the scheduling overhead will dwarf any gains from running on several cores.

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
Not unlike ctx_for, ctx_invoke calls all the functions it's passed in parallel.

No function call scheduled by ctx_invoke, nor any iteration of ctx_for, should ever access
a memory address modified by any other call/iteration - that is, they should be **completely independent**.
Once ctx_for/invoke returns, all the memory updates done by all the iterations/function calls can be used by the caller of
ctx_for/invoke.

Now a C89 example:

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
That last "0" is a null pointer to a *canceller* - we'll get to that in a moment. Meanwhile, parallel invoke in C89:

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
The tasks[] array should have {0,0} as its last element. Again, the "0" argument is the canceller.

Speaking of which - here's an example of actually using cancelling:

```C++
int pos_of_77 = -1;
ct_canceller* c = ct_alloc_canceller();
ctx_for(N, [&](int i) {
    if(arr[i] == 77) {
        pos_of_77 = i;
        ct_cancel(c);
    }
}, c);
ct_free_canceller(c);
```
(Again a silly piece of code doing way too little work per index, but no matter.) Notes on cancelling:

* **Everything can be cancelled**: ct_for, ctx_for, ct_invoke, and ctx_invoke can all get a canceller parameter.
* **A single canceller can cancel many things**: ct_cancel(c) cancels all loops and parallel calls to which c
  was originally passed.
* **Nested loops/calls don't automatically inherit the canceller**: when a loop is cancelled, no more iterations
  will be scheduled - but all iterations which are already in flight will be completed. If such an iteration
  itself spawns tasks, then those tasks will *not* be canceled - unless the spawning iteration explicitly passed
  to the tasks it spawned the same canceller which cancelled the loop that the spawner belongs to.
* **At most one iteration/function call can write something to memory** - otherwise, different results might be produced
  depending on timing, because cancelling is not deterministic (different iterations may
  be cancelled in different runs). For instance, the example above is only correct if arr[] is known to keep
  at most one value equal to 77.
  
The last thing to note is that you need, before using checkedthreads, to call **ct_init()** - and then
call **ct_fini()** when you're done. ct_init gets a single argument - the environment; for example:

```C
ct_env_var env[] = {
    {"CT_SCHED", "shuffle"},
    {"CT_RAND_REV", "1"},
    {0, 0}
};
ct_init(env);
```
You can pass 0 instead of env; if you do that, $CT_SCHED and $CT_RAND_REV will be looked up using getenv().
Similarly, if you do pass an env[], all variables not mentioned in it will be getenv()d.

The available environment variables and their meaning are discussed in the next section.

Environment variables
=====================

**$CT_SCHED** is the scheduler to use, and can be one of:

* **serial**: run loops serially from 0 to N and call functions first to last.
* **shuffle**: serial run with a pseudo-random, deterministic order of iterations and function calls.
* **valgrind**: same order as shuffle, but also communicates with the Valgrind checker, telling it what's what.
* **tbb**: schedule tasks using TBB's *simple_partitioner* with grain size of 1.
* **openmp**: schedule tasks using OpenMP's *#pragma omp parallel for schedule(dynamic,1)*.
* **pthreads** (default): schedule tasks using a worker pool of pthreads and a single shared queue.

**$CT_THREADS** is the worker pool size (relevant for the parallel schedulers); the default is a thread per core.

**$CT_VERBOSE**: at 2, all indexes are printed; at 1, loops/invokes; at 0 (default), nothing is printed.

**$CT_RAND_SEED**: a seed for order-randomizing schedulers (shuffle & valgrind).

**$CT_RAND_REV**: if non-zero, order-randomizing schedulers will reverse their random index permutations.
When this is useful is explained in the next section.

How race detection works
========================

As mentioned above, there are two verification methods - a fast one and a thorough one.

The fast one is, run the program twice using two different serial schedules - a random schedule and
then the same schedule, reversed:

```
env CT_SCHED=shuffle CT_RAND_REV=0 your-program your-arguments
env CT_SCHED=shuffle CT_RAND_REV=1 your-program your-arguments
```

Now compare the results of the two runs. Different results indicate a bug, because results should not
be affected by scheduling order (in production, a parallel scheduler is used and it can result in things
running in any of the two orders you just tried - and many other orders).

Using this method, you can run the program on many inputs (the program runs serially with the shuffle
scheduler, so you can spawn a process per core to fully utilize machines used for testing). Many inputs
and no result differences give you a rather high confidence that your program is correct.

However, this method has two drawbacks:

* **Some bugs go unnoticed**. For instance, updating a shared accumulator from several iterations of a loop
  may not work with a parallel scheduler. But such updates will yield the same results under all serial schedules.
  So will the use of a shared temporary buffer.
* **Bugs are not pinpointed**. Different results prove that there's a bug - but they don't tell you where it is.

Because of these drawbacks, a second, slower and more thorough verification method is available:

```
env CT_SCHED=valgrind CT_RAND_REV=0 valgrind --tool=checkedthreads your-program your-arguments
env CT_SCHED=valgrind CT_RAND_REV=1 valgrind --tool=checkedthreads your-program your-arguments
```

(If Valgrind says "failed to start tool 'checkedthreads'", perhaps **$VALGRIND_LIB** should be set
to point to the right place.)

This runs Valgrind with the checkedthreads tool, which monitors every memory access. When a thread accesses
a location that another thread concurrently wrote, the tool prints the offending call stack:

```
checkedthreads: error - thread 56 accessed 0x7FF000340 [0x7FF000340,4], owned by 55
==2919==    at 0x40202C: std::_Function_handler<void (int), main::{lambda(int)#1}>::_M_invoke(std::_Any_data const&, int) (bug.cpp:16)
==2919==    by 0x403293: ct_valgrind_for_loop (valgrind_imp.c:62)
==2919==    by 0x4031C8: ct_valgrind_for (valgrind_imp.c:82)
==2919==    by 0x40283C: ct_for (ct_api.c:177)
==2919==    by 0x401E9D: main (bug.cpp:20)
```

Note that there aren't any actual threads - like the run under CT_SCHED=shuffle, this run is serial.
Rather, the Valgrind tool maps ct_for loop indexes and ct_invoke function calls to thread IDs, such that those
IDs can fit into a single byte. So "two threads accessing the same location" means that the location
was accessed from two loop indexes/function calls that could run in parallel.

This second method is slower, but it **doesn't miss any bugs that could ever occur with the given inputs** -
and it **pinpoints** the bugs. So it's a good idea to run the program under Valgrind on a few inputs
in case plain shuffling misses bugs. And it's also useful to run the program under Valgrind on those inputs
where shuffling discovered bugs - to pinpoint those bugs.

This is all you strictly need to know to verify your code. If you want more details - for instance,
if you want to be convinced that the bug coverage is indeed as thorough as claimed above - you can read
a detailed explanation [here](http://yosefk.com/blog/checkedthreads-bug-free-shared-memory-parallelism.html).

Note that there can be no deadlocks, because there are no locks (the only way to synchronize threads is forking -
the spawning of loops - or joining, the termination of loops). So there's no need for deadlock detection tools.

Building and installing
=======================

It is perhaps possible to [download binaries](http://yosefk.com/checkedthreads/binaries.html) for your platform - something I always prefer to do...
It is recommended that you read the following explanations about building from sources, and then
decide which parts you want to build and which parts you prefer to download in binary form.

So, after cloning/downloading sources:

```
cd checkedthreads
./configure
```

**configure** is a Python script producing a single output, **include/checkedthreads_config.h**. You can
edit this file manually; all it has is #defines telling which of the optional features are enabled.
Note that make then *checks what's #defined in config.h* and adjusts compiler flags, etc.

* **#define CT_CXX11** - enable C++11 (the C++ parts of the code are compiled with -std=c++0x).
* **#define CT_OPENMP** - enable OpenMP (code is compiled with -fopenmp).
* **#define CT_TBB** - enable TBB.
* **#define CT_PTHREAD** - enable pthreads (code is compiled with -pthread).

./configure enables each of these features if it auto-detects that it's supported on your machine.
You might then want to disable some features (even though they're supported on your machine).

At this point, you can build the libraries with **make build**, but plain **make** won't work yet because
things must be manually configured to build the Valgrind tool. To do that, edit **defaults.mk** and
set the following variables:

* **CT_VALGRIND_SRC_DIR**: the directory with the Valgrind sources. To build the tool, you need
to download, configure and make Valgrind first. The Valgrind tool was developed with **Valgrind 3.8.1**;
the binaries seem to work with 3.7.0 as well but I didn't try to build with the 3.7.0 sources.
You can download the 3.8.1 sources from [here](http://www.valgrind.org/downloads/valgrind-3.8.1.tar.bz2) or
[here](http://yosefk.com/checkedthreads/valgrind-3.8.1.tar.bz2).
* **VALGRIND_LIB**: your system's default (such as /usr/lib/valgrind), or the directory you passed
to Valgrind's configure with --prefix, or some other (existing) directory if you wish. The checkedthread tool binaries
are copied to this directory.
* **CT_VALGRIND_CP**: *sudo cp* by default - can be plain cp if you have permissions to access $VALGRIND_LIB
without sudo.

Note that **$VALGRIND_LIB** is used by the tests which make runs after building everything - make
sets this environment variable for all the processes it spawns. You may need to explicitly set $VALGRIND_LIB
when running *valgrind --tool=checkedthreads* outside make.

Also note that setting, in the shell running make, any of the environment variables mentioned
in defaults.mk **overrides** their values (hence the name "defaults.mk").

With defaults.mk edited, you can build everything and run tests with:

```
make
```

**make help** will list the available make targets and options (such as **make clean** and **make VERBOSE=1**).
Currently every build rebuilds everything from scratch (there's
no dependency checking), which is tolerable at the current size of things.

After a successful build, you get libraries at **lib/** as follows:

* Every library is available both as a static **lib*.a** file a dynamic **lib*.so** file.
* **libcheckedthreads++** has all the enabled features.
* **libcheckedthreads** has all the enabled features except those relying on C++ (the C++11 API and the TBB-based scheduler).
* If OpenMP is enabled, **libcheckedthreads++_openmp** is created that has all the enabled features but only one parallel scheduler,
the one based on OpenMP. **libcheckedthreads_openmp** is similar, except that it also doesn't use C++.
* Similarly, if pthreads are enabled, **libcheckedthreads++_pthreads** and **libcheckedthreads_pthreads**
are built.
* Similarly, if TBB is enabled, **libcheckedthreads++_tbb** is built (but *not* libcheckedthreads_tbb, because
TBB requires C++).

This variety of libraries should hopefully make it easy to link with checkedthreads in any scenario...

At this point you can "install" checkedthreads - that is, copy the files in **lib/** to wherever you keep
your libraries, and copy the files in **include/** to wherever you keep your header files.

If you want to download binaries instead of building, the available binaries are listed
[here](http://yosefk.com/checkedthreads/binaries.html). You can choose to download just the Valgrind binaries
(the slightly gnarlier thing to build) but to build the libraries from sources, for example.

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
* Valgrind tool code (at valgrind/) should try to use Valgrind style.
* Code should compile without warnings (I'd use -Werror, but different gcc versions have different warnings.)
* Style isn't that important.

Support/contact
===============

<Yossi.Kreinin@gmail.com>/<http://yosefk.com>. Feel free to contact if you run into any sort of problem using checkedthreads.
