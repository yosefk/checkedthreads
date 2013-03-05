import re

def conflicting_threads(error_message):
    return sorted([int(i) for i in re.match('.*thread (\d+) accessed .* owned by (\d+).*',error_message).groups()])

def conflict_options(i,j):
    jcon = [[j,j+1]]*10
    icon1 = [[0,i],sorted([i,j])]
    icon2 = [[i,i+1],sorted([i+1,j])]
    return sorted(jcon+icon1),sorted(jcon+icon2)

hadfailures = False
for r in [0,1]:
    for i,j in [(3,8), (5,1)]:
        s, o, c = runcommand('env CT_SCHED=valgrind CT_RAND_REV=%d valgrind --tool=checkedthreads ./bin/nested %d %d'%(r,i,j),expected_status=None)
        conflicts = sorted([conflicting_threads(x) for x in o.split('\n') if 'owned by' in x])
        if conflicts not in conflict_options(i,j):
            fail(c)
            hadfailures = True
if verbose and not hadfailures:
  print ' ','expected inter-thread conflicts detected'
