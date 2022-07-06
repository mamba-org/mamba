#!/usr/bin/env bash

set -euo pipefail -x

# Directory of this file
readonly __DIR__="$(cd "$(dirname "${BASH_SOURCE[0]:?}")" && pwd)"
# reposerver python script
readonly reposerver="${__DIR__}/reposerver.py"
# Conda mock repository
readonly repo="${__DIR__}/repo/"

# Set up a temporary space for Conda environment and packages
readonly test_dir="$(mktemp -d -t mamba-test-reposerver-XXXXXXXXXX)"
export CONDA_ENVS_PATH="${test_dir}/envs"
export CONDA_PKGS_DIRS="${test_dir}/pkgs"
readonly this_pid="$$"
# On exit, kill all subprocess and cleanup test directory.
trap 'rm -rf "${test_dir}"; pkill -P ${this_pid} || true' EXIT

# 10 random characters
function make-name {
	LC_ALL=C tr -dc A-Za-z0-9 </dev/urandom | head -c 10
}


python "${reposerver}" -d "${repo}" --auth none & PID=$!
mamba create -y -q -n "$(make-name)" --override-channels -c http://localhost:8000/ test-package --json
kill -TERM $PID

export TESTPWD="user:test"
python "${reposerver}" -d "${repo}" --auth basic & PID=$!
mamba create -y -q -n "$(make-name)" --override-channels -c http://user:test@localhost:8000/ test-package --json
kill -TERM $PID

export TESTPWD="user@email.com:test"
python "${reposerver}" -d "${repo}" --auth basic & PID=$!
mamba create -y -q -n "$(make-name)" --override-channels -c http://user@email.com:test@localhost:8000/ test-package --json
kill -TERM $PID

unset TESTPWD
python "${reposerver}" -d "${repo}" --token xy-12345678-1234-1234-1234-123456789012 & PID=$!
mamba create -y -q -n "$(make-name)" --override-channels -c http://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012 test-package --json
kill -TERM $PID

if [[ "$(uname -s)" == "Linux" ]]; then
	export KEY1=$(gpg --fingerprint "MAMBA1")
	export KEY2=$(gpg --fingerprint "MAMBA2")

	python "${reposerver}" -d "${repo}" --auth none --sign & PID=$!
	sleep 5s
	kill -TERM $PID
fi

export TESTPWD=":test"
python "${reposerver}" -d "${repo}" --auth basic --port 8005 & PID=$!
python "${reposerver}" -d "${repo}" --auth basic --port 8006 & PID2=$!
python "${reposerver}" -d "${repo}" --auth basic --port 8007 & PID3=$!
mamba create -y -q -n "$(make-name)" --override-channels -c http://:test@localhost:8005/ -c http://:test@localhost:8006/ -c http://:test@localhost:8007/ test-package --json
kill -TERM $PID
kill -TERM $PID2
kill -TERM $PID3
