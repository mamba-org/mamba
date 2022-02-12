#!/bin/bash

set -euo pipefail -x

rm -rf $CONDA_PREFIX/pkgs/test-package*
ENV_NAME=testauth

export TESTPWD=":test"
python reposerver.py -d repo/ --auth basic --port 8005 & PID=$!
python reposerver.py -d repo/ --auth basic --port 8006 & PID=$!
python reposerver.py -d repo/ --auth basic --port 8007 & PID=$!
mamba create -y -k -vvv -n $ENV_NAME --override-channels -c http://:test@localhost:8005/ -c http://:test@localhost:8006/ -c http://:test@localhost:8007/ test-package --json
kill -TERM $PID
kill -TERM $PID2
kill -TERM $PID3
rm -rf $CONDA_PREFIX/envs/$ENV_NAME
