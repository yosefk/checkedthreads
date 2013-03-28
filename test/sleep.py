for sched in scheds:
    if sched not in 'serial shuffle valgrind'.split():
        runtest('sleep',CT_SCHED=sched)

