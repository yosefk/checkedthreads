#!/usr/bin/python
import sys, os, commands

print 'building the valgrind tool'
print

verbose = int(os.getenv('VERBOSE',0))

valgrind_root = os.getenv('CT_VALGRIND_SRC_DIR')
if not valgrind_root:
    print 'please edit defaults.mk so that $CT_VALGRIND_SRC_DIR points to your Valgrind source distribution'
    sys.exit(1)

lackey = os.path.join(valgrind_root,'lackey')
makefile = os.path.join(lackey, 'Makefile')
if not os.path.isdir(lackey):
    print valgrind_root,'does not look like a valgrind source root directory (lackey tool directory not found)'
    sys.exit(1)

if not os.path.exists(makefile):
    print valgrind_root,"does not look like it was configure'd and make'd already (lackey/Makefile not found)"
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

runcmd('cp valgrind/checkedthreads_main.c %s/lk_main.c && make -C %s'%(lackey,lackey),'compilation of checkedthreads_main.c failed')

outfile = commands.getoutput('ls -tr %s | tail -1'%lackey)
if not outfile.startswith('lackey-'):
    print 'error - expected a file named lackey-<architecture-name> to have been created'
    sys.exit(1)

destfile = outfile.replace('lackey-','checkedthreads-')

valgrind_lib = os.getenv('VALGRIND_LIB')
valgrind_cp = os.getenv('CT_VALGRIND_CP')

runcmd(valgrind_cp+' '+os.path.join(lackey,outfile)+' '+os.path.join(valgrind_lib,destfile),
        'copying the checkedthreads tool to the Valgrind library directory failed')
