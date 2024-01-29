#!/usr/bin/env bash

set -euo pipefail -x

# Directory of this file
readonly __DIR__="$(cd "$(dirname "${BASH_SOURCE[0]:?}")" && pwd)"
# reposerver python script
readonly reposerver="${__DIR__}/reposerver.py"
# Conda mock repository
readonly repo="${__DIR__}/repo/"

# Default value "mamba" for executable under test
export TEST_MAMBA_EXE="${TEST_MAMBA_EXE:-micromamba}"

# Avoid externally configured .condarc file
unset CONDARC

# Set up a temporary space for Conda environment and packages
readonly test_dir="$(mktemp -d -t mamba-test-reposerver-XXXXXXXXXX)"
export CONDA_ENVS_DIRS="${test_dir}/envs"
export CONDA_PKGS_DIRS="${test_dir}/pkgs"
readonly this_pid="$$"
# On exit, kill all subprocess and cleanup test directory.
trap 'rm -rf "${test_dir}"; pkill -P ${this_pid} || true' EXIT


start_server() {
	exec python "${reposerver}" -n mychannel -d "${repo}" "$@"
}

test_install() {
	local tmp=$(mktemp -d)
	local condarc="${tmp}/condarc"
	"${TEST_MAMBA_EXE}" create -y -p "${tmp}/env1" --override-channels -c $1/mychannel test-package --json
	cat > "${condarc}" <<EOF
override_channels: true
channels: [mychannel]
channel_alias: "$1"
EOF
        cat "${condarc}" >&2
	"${TEST_MAMBA_EXE}" create -y -p "${tmp}/env2" test-package --json --rc-file "${condarc}" >&2
}


start_server & PID=$!
test_install http://localhost:8000 test-package --json
kill -TERM $PID

start_server --auth basic --user user --password test & PID=$!
test_install http://user:test@localhost:8000 test-package --json
kill -TERM $PID

start_server --auth basic --user user@email.com --password test & PID=$!
test_install http://user%40email.com:test@localhost:8000 test-package --json
kill -TERM $PID

start_server --token xy-12345678-1234-1234-1234-123456789012 & PID=$!
test_install http://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012 test-package --json
kill -TERM $PID

if [[ "$(uname -s)" == "Linux" ]]; then
	export KEY1=$(gpg --fingerprint "MAMBA1")
	export KEY2=$(gpg --fingerprint "MAMBA2")

	start_server --auth none --sign & PID=$!
	sleep 5s
	kill -TERM $PID
fi

python "${reposerver}" -d "${repo}" --auth basic --user user --password test --port 8005 & PID=$!
python "${reposerver}" -d "${repo}" --auth basic --user user --password test --port 8006 & PID2=$!
python "${reposerver}" -d "${repo}" --auth basic --user user --password test --port 8007 & PID3=$!
"${TEST_MAMBA_EXE}" create -y -q -n "env-${RANDOM}" --override-channels -c http://user:test@localhost:8005/ -c http://user:test@localhost:8006/ -c http://user:test@localhost:8007/ test-package --json
kill -TERM $PID
kill -TERM $PID2
kill -TERM $PID3

readonly channel_a="${__DIR__}/channel_a/"
readonly channel_b="${__DIR__}/channel_b/"
python "${reposerver}" \
	-d "${repo}" -n defaults --token private-token -- \
	-d "${channel_a}" -n channel_a --user user@email.com --password test -- \
	-d "${channel_b}" -n channel_b --auth none & PID=$!
"${TEST_MAMBA_EXE}" create -y -q -n "env-${RANDOM}" --override-channels -c http://localhost:8000/t/private-token/defaults test-package --json
"${TEST_MAMBA_EXE}" create -y -q -n "env-${RANDOM}" --override-channels -c http://user%40email.com:test@localhost:8000/channel_a _r-mutex --json
kill -TERM $PID
