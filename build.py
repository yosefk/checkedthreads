#!/usr/bin/python

dirs = 'obj lib bin'.split()
srcsc = 'ct_api.c serial_imp.c pthreads_imp.c openmp_imp.c shuffle_imp.c valgrind_imp.c'.split()
srcsxx = 'ctx_api.cpp tbb_imp.cpp'.split()
libc = 'checkedthreads'
libxx = 'checkedthreads++'
tests = 'hello_ct.c hello_ctx.cpp'.split()

# utilities
###########

import sys, os

verbose = int(os.getenv('VERBOSE',0))

def build():
    for dir in dirs:
        mkdir(dir)
    for lib,srcs,more in ((libc,srcsc+['tbb_stub.c'],[]),(libxx,srcsxx,srcsc)):
        print '\nbuilding','lib'+lib
        for src in srcs:
            compile(src)
        link(lib,srcs+more)
    print '\nbuilding tests'
    for test in tests:
        buildtest(test)

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
        'g++':'-std=c++0x', #TODO: configure
    }[compiler] + ' -pedantic -Wall -Wextra -Werror -g -fopenmp'

def compiler(fname):
    ext = fname.split('.')[-1]
    return {'c':'gcc','cpp':'g++'}[ext]

def compile(fname):
    src = 'src/'+fname
    obj = 'obj/'+fname+'.o'
    update('%s -c %s -o %s -I include'%(compiler(fname),src,obj),[obj],[src])

def link(libname,srcs):
    lib = 'lib/lib%s.a'%libname
    objs = ['obj/%s.o'%src for src in srcs]
    #update('g++ -o lib/lib%s.so -shared -fPIC %s'%(so,' '.join(objs)),[so],objs)
    update('ar cr %s %s'%(lib,' '.join(objs)),[lib],objs)

def buildtest(test):
    name = test.split('.')[0]
    bin = 'bin/'+name
    src = 'test/'+test
    cc = compiler(test)
    lib = {'gcc':libc,'g++':libxx}[cc]
    update('%s %s -o %s lib/lib%s.a -ltbb -I include'%(cc,src,bin,lib),[bin],[src])

if __name__ == '__main__':
    build()
