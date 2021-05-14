#!/bin/bash

set -euo pipefail -x

rm -rf $CONDA_PREFIX/pkgs/test-package*
ENV_NAME=testauth

python reposerver.py -d repo/ --auth none & PID=$!
mamba create -y -q -n $ENV_NAME --override-channels -c http://localhost:8000/ test-package --json
kill -TERM $PID
rm -rf $CONDA_PREFIX/envs/$ENV_NAME

python reposerver.py -d repo/ --auth basic & PID=$!
mamba create -y -q -n $ENV_NAME --override-channels -c http://user:test@localhost:8000/ test-package --json
kill -TERM $PID
rm -rf $CONDA_PREFIX/envs/$ENV_NAME

python reposerver.py -d repo/ --auth token & PID=$!
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
