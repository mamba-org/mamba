#!/bin/bash

set -euo pipefail -x

rm -rf $CONDA_PREFIX/pkgs/test-package*
ENV_NAME=testauth

python reposerver.py -d repo/ --auth none & PID=$!
mamba create -y -q -n $ENV_NAME --override-channels -c http://localhost:8000/ test-package --json
kill -TERM $PID
rm -rf $CONDA_PREFIX/envs/$ENV_NAME

export TESTPWD="user:test"
python reposerver.py -d repo/ --auth basic & PID=$!
mamba create -y -q -n $ENV_NAME --override-channels -c http://user:test@localhost:8000/ test-package --json
kill -TERM $PID
rm -rf $CONDA_PREFIX/envs/$ENV_NAME

export TESTPWD="user@email.com:test"
python reposerver.py -d repo/ --auth basic & PID=$!
mamba create -y -q -n $ENV_NAME --override-channels -c http://user@email.com:test@localhost:8000/ test-package --json
kill -TERM $PID
rm -rf $CONDA_PREFIX/envs/$ENV_NAME

python reposerver.py -d repo/ --token xy-12345678-1234-1234-1234-123456789012 & PID=$!
mamba create -y -q -n $ENV_NAME --override-channels -c http://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012 test-package --json
kill -TERM $PID
rm -rf $CONDA_PREFIX/envs/$ENV_NAME

if [[ "$(uname -s)" == "Linux" ]]; then
	export KEY1=$(gpg --fingerprint "MAMBA1")
	export KEY2=$(gpg --fingerprint "MAMBA2")

	python reposerver.py -d repo/ --auth none --sign & PID=$!
	sleep 5s
	kill -TERM $PID
fi

export TESTPWD=":test"
python reposerver.py -d repo/ --auth basic --port 8005 & PID=$!
python reposerver.py -d repo/ --auth basic --port 8006 & PID2=$!
python reposerver.py -d repo/ --auth basic --port 8007 & PID3=$!
mamba create -y -q -n $ENV_NAME --override-channels -c http://:test@localhost:8005/ -c http://:test@localhost:8006/ -c http://:test@localhost:8007/ test-package --json
kill -TERM $PID
kill -TERM $PID2
kill -TERM $PID3
rm -rf $CONDA_PREFIX/envs/$ENV_NAME
