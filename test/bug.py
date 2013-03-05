# bug: one of the random orders should find the bug, with and without valgrind;
# the reverse order /shouldn't/ find the bug
s1, o1, c1 = runtest('bug',expected_status=None,CT_SCHED='shuffle')
s2, o2, c2 = runtest('bug',expected_status=None,CT_SCHED='shuffle',CT_RAND_REV=1)
if s1 == s2 or (s1 != 0 and s2 != 0):
    fail(c1)
    fail(c2)
elif verbose:
    print ' ','bug found when running one of the random orders'

# run bug under valgrind:
s1, o1, c1 = runcommand('env CT_SCHED=valgrind valgrind --tool=checkedthreads ./bin/bug',expected_status=None)
s2, o2, c2 = runcommand('env CT_SCHED=valgrind CT_RAND_REV=1 valgrind --tool=checkedthreads ./bin/bug',expected_status=None)
loc1='bug.cpp:16'
loc2='bug.cpp:18'
if not ((loc1 in o1 and loc2 in o2) or \
        (loc2 in o1 and loc1 in o2)):
    fail(c1)
    fail(c2)
elif verbose:
    print ' ','bug found when running either of the random orders'
