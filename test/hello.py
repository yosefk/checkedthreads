# hello: all builds should run, and all schedulers should be available in the "all-scheduler" build
hellos = [t for t in built if t.startswith('hello')]
hello_output = 'results: 0 3 6 ... 291 294 297'
for hello in hellos:
    runtest(hello,expected_output=hello_output)
for sched in scheds:
    sched_tests = []
    if with_cpp:
        sched_tests.append('hello_ctx')
    if sched != 'tbb':
        sched_tests.append('hello_ct')
    for sched_test in sched_tests:
        runtest(sched_test,expected_output=hello_output,CT_SCHED=sched)

