

"${TEST_MAMBA_EXE}" run -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


"${TEST_MAMBA_EXE}" run -a "stdin stderr stdout" -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdin stderr stdout" -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdin stderr stdout" -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


"${TEST_MAMBA_EXE}" run -a "stderr stdout" -n base /bin/bash -c "test -t 0"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stderr stdout" -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stderr stdout" -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


"${TEST_MAMBA_EXE}" run -a "stdin stderr" -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdin stderr" -n base /bin/bash -c "test -t 1"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdin stderr" -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


"${TEST_MAMBA_EXE}" run -a "stdin stdout" -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdin stdout" -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdin stdout" -n base /bin/bash -c "test -t 2"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)


"${TEST_MAMBA_EXE}" run -a "stdout" -n base /bin/bash -c "test -t 0"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdout" -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdout" -n base /bin/bash -c "test -t 2"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)


"${TEST_MAMBA_EXE}" run -a "stdin" -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdin" -n base /bin/bash -c "test -t 1"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stdin" -n base /bin/bash -c "test -t 2"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)


"${TEST_MAMBA_EXE}" run -a "stderr" -n base /bin/bash -c "test -t 0"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stderr" -n base /bin/bash -c "test -t 1"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "stderr" -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


"${TEST_MAMBA_EXE}" run -a "" -n base /bin/bash -c "test -t 0"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "" -n base /bin/bash -c "test -t 1"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

"${TEST_MAMBA_EXE}" run -a "" -n base /bin/bash -c "test -t 2"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)
