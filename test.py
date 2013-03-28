#!/usr/bin/python
'''stuff we test:
* "hello" should work with all enabled schedulers and link against all single-scheduler libraries.
* "sleep" should sleep "quickly" with all enabled schedulers (partitioning test)
* random checker should find bugs.
* valgrind checker should find bugs.
* various stuff - like find, sort and accumulate.
'''
import os
import sys
import build
import commands

tests = 'bug.cpp sleep.cpp nested.cpp grain.cpp acc.cpp cancel.cpp sort.cpp'.split()

with_cpp = 'C++11' in build.enabled
with_pthreads = 'pthreads' in build.enabled
with_openmp = 'OpenMP' in build.enabled
with_tbb = 'TBB' in build.enabled

print '\nbuilding tests'

verbose = build.verbose
built = []
def buildtest(*args):
    built.append(build.buildtest(*args))

buildtest('hello_ct.c')
if with_pthreads: buildtest('hello_ct.c','_pthreads')
if with_openmp: buildtest('hello_ct.c','_openmp')

if with_cpp:
    buildtest('hello_ctx.cpp')
    if with_pthreads: buildtest('hello_ctx.cpp','_pthreads')
    if with_openmp: buildtest('hello_ctx.cpp','_openmp')
    if with_tbb: buildtest('hello_ctx.cpp','_tbb')

for test in tests:
    if test.endswith('.cpp') and not with_cpp:
        continue
    buildtest(test)

scheds = 'serial shuffle valgrind openmp tbb pthreads'.split()
# remove schedulers which we aren't configured to support
def lower(ls): return [s.lower() for s in ls]
scheds = [sched for sched in scheds if not (sched in lower(build.features) \
                                        and sched not in lower(build.enabled))]

failed = []
def fail(command):
    print ' ',command,'FAILED'
    failed.append(command)

def runtest(name,args='',expected_status=0,expected_output=None,**env):
    envstr=' '.join(['%s=%s'%(n,v) for n,v in env.items()])
    command = 'env %s ./bin/%s %s'%(envstr,name,args)
    return runcommand(command,expected_status,expected_output)

def runcommand(command,expected_status=0,expected_output=None):
    if verbose:
        print ' ','running',command
    status,output = commands.getstatusoutput(command)
    if verbose>1:
        print '   ','\n    '.join(output.split('\n'))
    bad_status = status != expected_status and expected_status != None
    bad_output = output != expected_output and expected_output != None
    if bad_status or bad_output:
        fail(command)
    return status, output, command

print '\nrunning tests'

testscripts = 'hello.py bug.py nested.py sleep.py'.split()

for testscript in testscripts:
    execfile('test/'+testscript)

for test in built:
    if test in 'bug nested sleep'.split() or test.startswith('hello'):
        continue
    if test == 'sort':
        runtest(test,args=str(1024*1024))
    else:
        runtest(test)

if failed:
    print 'FAILED:'
    print '\n'.join(failed)
    sys.exit(1)
