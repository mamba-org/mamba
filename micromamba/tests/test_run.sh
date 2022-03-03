

micromamba run -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


micromamba run -a "stdin stderr stdout" -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdin stderr stdout" -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdin stderr stdout" -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


micromamba run -a "stderr stdout" -n base /bin/bash -c "test -t 0"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stderr stdout" -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stderr stdout" -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


micromamba run -a "stdin stderr" -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdin stderr" -n base /bin/bash -c "test -t 1"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdin stderr" -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


micromamba run -a "stdin stdout" -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdin stdout" -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdin stdout" -n base /bin/bash -c "test -t 2"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)


micromamba run -a "stdout" -n base /bin/bash -c "test -t 0"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdout" -n base /bin/bash -c "test -t 1"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdout" -n base /bin/bash -c "test -t 2"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)


micromamba run -a "stdin" -n base /bin/bash -c "test -t 0"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdin" -n base /bin/bash -c "test -t 1"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stdin" -n base /bin/bash -c "test -t 2"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)


micromamba run -a "stderr" -n base /bin/bash -c "test -t 0"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stderr" -n base /bin/bash -c "test -t 1"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "stderr" -n base /bin/bash -c "test -t 2"
test $? -eq 0 && echo "ok" || (echo "fail" ; exit 1)


micromamba run -a "" -n base /bin/bash -c "test -t 0"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "" -n base /bin/bash -c "test -t 1"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)

micromamba run -a "" -n base /bin/bash -c "test -t 2"
test $? -eq 1 && echo "ok" || (echo "fail" ; exit 1)
