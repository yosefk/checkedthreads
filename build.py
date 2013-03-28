#!/usr/bin/python
'''
Here's what we build:

libcheckedthreads.a - a C library with all features enabled by checkedthreads_config.h.
libcheckedthreads_pthreads.a - built if pthreads are enabled; has a single parallel scheduler based on pthreads.
libcheckedthreads_openmp.a - built if OpenMP is enabled; has a single parallel scheduler based on OpenMP.

We also build:

libcheckedthreads++.a
libcheckedthreads++_pthreads.a
libcheckedthreads++_openmp.a
libcheckedthreads++_tbb.a (TBB is like pthreads and OpenMP, except for being a C++ library.)

...which are like the versions without "++" with the addition of a C++ API. (Note that the API
may depend on C++11 and thus will not be available without C++11 support.) TBB (a C++ library)
is only available in libcheckedthreads++ and libcheckedthreads++_tbb.

The purpose of building the single-parallel-scheduler versions is, just because someone has
TBB libraries and gcc with OpenMP support installed doesn't mean they want to always build
everything with OpenMP support *and* link against the TBB libraries. So if someone has an OpenMP
app and they want checkedthreads to use the OpenMP scheduler, they can link against libcheckedthreads_openmp.a,
without having to carry the other schedulers and appeasing the tools into building their app with
those schedulers built into checkedthreads.
'''

dirs = 'obj lib bin'.split()
srcsc = 'ct_api.c serial_imp.c pthreads_imp.c openmp_imp.c shuffle_imp.c valgrind_imp.c'.split() +\
        'lock_based_queue.c nprocs.c work_item.c'.split()
srcsxx = 'ctx_api.cpp tbb_imp.cpp'.split()
libc = 'checkedthreads'
libxx = 'checkedthreads++'

# utilities
###########

import sys, os, commands

verbose = int(os.getenv('VERBOSE',0))

features = {
    'C++11': dict(),
    'OpenMP': dict(compiler_flags='-fopenmp'),
    'TBB': dict(linker_flags='-ltbb'), 
    'pthreads': dict(compiler_flags='-pthread'),
}

def enabled_features():
    '''ugly, but nice in the sense that one can simply edit include/checkedthreads_config.h
    and the change will propagate to build.py's view of what features are supported.
    we figure out which #defines are set by running cpp on cfg/features.c'''
    cppout = commands.getoutput('cpp cfg/features.c -I include')
    return sorted([feature for feature in features if feature+' enabled' in cppout])

enabled = enabled_features()

def attr(feature,attrname,default=''):
    return features[feature].get(attrname,default) if feature in enabled else default

def all_enabled(attrname,default=''):
    return ' '.join([attr(feature,attrname,default) for feature in enabled])

def stub_out_all_but(feature,srcs):
    osrcs = []
    for src in srcs:
        ignore = False
        for f in features:
            if f != feature and f.lower()+'_imp' in src:
                ignore = True
                break
        if not ignore:
            osrcs.append(src)
    return osrcs + ['stubs_%s.c'%feature.lower()]

def build():
    print 'enabled features:',' '.join(enabled_features())
    for dir in dirs:
        mkdir(dir)
    for lib,srcs,more in ((libc,srcsc+['tbb_stub.c'],[]),(libxx,srcsxx,srcsc)):
        if '++' in lib and 'C++11' not in enabled:
            continue
        print '\nbuilding','lib'+lib
        for src in srcs:
            compile(src)
        for shared in (True,False):
            link(lib,srcs+more,shared)
        # single-scheduler libraries
        for feature in ['OpenMP','TBB','pthreads']:
            if feature in enabled:
                if feature == 'TBB' and '++' not in lib:
                    continue
                singlelib = lib + '_' + feature.lower()
                print '\nbuilding','lib'+singlelib
                compile('stubs_%s.c'%feature.lower())
                for shared in (True,False):
                    link(singlelib,stub_out_all_but(feature,srcs+more),shared)

def update(cmd,outputs=[],inputs=[]):
    '''TODO: check inputs & outputs timestamps'''
    try:
        cmd = cmd + ' ' + flags(cmd.split()[0])
    except:
        pass # not a compilation command       
    if verbose>0 and (inputs or outputs):
        print ' ',' '.join(inputs),'->',' '.join(outputs)
    if verbose>1:
        print '$',cmd
    status = os.system(cmd)
    if status != 0:
        print
        print 'failed building',' '.join(outputs)
        print '`%s` failed with status %d'%(cmd,status)
        sys.exit(1)

def mkdir(name):
    update('mkdir -p '+name)

def flags(compiler):
    return {
        'gcc':'-std=c89',
        'g++':'-std=' + ('c++0x' if 'C++11' in enabled else 'c++98'),
    }[compiler] + ' -pedantic -Wall -Wextra -Wno-unused -g -O3 ' + all_enabled('compiler_flags')

def compiler(fname):
    ext = fname.split('.')[-1]
    return {'c':'gcc','cpp':'g++'}[ext]

def compile(fname):
    src = 'src/'+fname
    obj = 'obj/'+fname+'.o'
    update('%s -c %s -o %s -fPIC -I include'%(compiler(fname),src,obj),[obj],[src])

def link(libname,srcs,shared=False):
    ext = ['a','so'][int(shared)]
    lib = 'lib/lib%s.%s'%(libname,ext)
    objs = ['obj/%s.o'%src for src in srcs]
    if shared:
        update('g++ -o %s -shared -fPIC %s'%(lib,' '.join(objs)),[lib],objs)
    else:
        update('ar cr %s %s'%(lib,' '.join(objs)),[lib],objs)

def buildtest(test,lib_postfix=''):
    name = test.split('.')[0]+lib_postfix
    bin = 'bin/'+name
    src = 'test/'+test
    cc = compiler(test)
    lib = {'gcc':libc,'g++':libxx}[cc]+lib_postfix
    update('%s %s -o %s lib/lib%s.a -I include %s'%(cc,src,bin,lib,all_enabled('linker_flags')),[bin],[src])
    return name

if __name__ == '__main__':
    build()
