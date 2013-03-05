#!/usr/bin/python
import sys, os, commands

print 'building the valgrind tool'
print

verbose = int(os.getenv('VERBOSE',0))

valgrind_root = os.getenv('CT_VALGRIND_SRC_DIR')
if not valgrind_root:
    print 'please edit defaults.mk so that $CT_VALGRIND_SRC_DIR points to your Valgrind source distribution'
    sys.exit(1)

massif = os.path.join(valgrind_root,'massif')
makefile = os.path.join(massif, 'Makefile')
if not os.path.isdir(massif):
    print valgrind_root,'does not look like a valgrind source root directory (massif tool directory not found)'
    sys.exit(1)

if not os.path.exists(makefile):
    print valgrind_root,"does not look like it was configure'd and make'd already (massif/Makefile not found)"
    sys.exit(1)

def runcmd(cmd,errmsg):
    if verbose>0:
        print '$',cmd
    if verbose<2:
        cmd += ' > /dev/null'
    status = os.system(cmd)
    
    if status:
        print errmsg
        sys.exit(1)

runcmd('cp valgrind/checkedthreads_main.c %s/ms_main.c && make -C %s'%(massif,massif),'compilation of checkedthreads_main.c failed')

def valgrind_copy(outfile):
    destfile = outfile.replace('massif-','checkedthreads-')
    
    valgrind_lib = os.getenv('VALGRIND_LIB')
    valgrind_cp = os.getenv('CT_VALGRIND_CP')
    
    runcmd(valgrind_cp+' '+os.path.join(massif,outfile)+' '+os.path.join(valgrind_lib,destfile),
            'copying the checkedthreads tool to the Valgrind library directory failed')

outfile = commands.getoutput('ls -tr %s | tail -1'%massif)
if not outfile.startswith('massif-'):
    print 'error - expected a file named massif-<architecture-name> to have been created'
    sys.exit(1)

preloadfile = commands.getoutput('ls %s | grep vgpreload'%massif)
if 'massif' not in outfile:
    print 'error - expected a file named vgpreload_massif-... to have been created'
    sys.exit(1)

valgrind_copy(outfile)
valgrind_copy(preloadfile)
