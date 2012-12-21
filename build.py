#!/usr/bin/python

dirs = 'obj lib bin'.split()
srcs = 'ct_api.c ctx_api.cpp serial_imp.c pthreads_imp.c valgrind_imp.c'.split()
lib = 'checkedthreads'
tests = 'hello_ct.c'.split()

# utilities
###########

import sys, os

def build():
    for dir in dirs:
        mkdir(dir)
    print '\nbuilding','lib'+lib
    for src in srcs:
        compile(src)
    link(lib,srcs)
    print '\nbuilding tests'
    for test in tests:
        buildtest(test)

def update(cmd,outputs=[],inputs=[]):
    '''TODO: check inputs & outputs timestamps'''
    try:
        cmd = cmd + ' ' + flags(cmd.split()[0])
    except:
        pass # not a compilation command       
    if inputs or outputs:
        print ' ',' '.join(inputs),'->',' '.join(outputs)
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
    }[compiler] + ' -pedantic -Wall -Wextra -Werror -g'

def compile(fname):
    ext = fname.split('.')[-1]
    compiler = {'c':'gcc','cpp':'g++'}[ext]
    src = 'src/'+fname
    obj = 'obj/'+fname+'.o'
    update('%s -c %s -o %s -I include'%(compiler,src,obj),[obj],[src])

def link(so,srcs):
    objs = ['obj/%s.o'%src for src in srcs]
    #update('g++ -o lib/lib%s.so -shared -fPIC %s'%(so,' '.join(objs)),[so],objs)
    update('ar cr lib/lib%s.a %s'%(so,' '.join(objs)),[so],objs)

def buildtest(test):
    name = test.split('.')[0]
    bin = 'bin/'+name
    src = 'test/'+test
    update('g++ %s -o %s lib/libcheckedthreads.a -I include'%(src,bin),[bin],[src])

if __name__ == '__main__':
    build()
