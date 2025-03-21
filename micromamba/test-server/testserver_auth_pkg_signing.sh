#!/usr/bin/env bash

set -euo pipefail -x

# Directory of this file
readonly __DIR__="$(cd "$(dirname "${BASH_SOURCE[0]:?}")" && pwd)"
# reposerver python script
readonly reposerver="${__DIR__}/reposerver.py"
# Conda mock repository
readonly repo="${__DIR__}/repo/"

# Default value "mamba"
export TEST_MAMBA_EXE="${TEST_MAMBA_EXE:-micromamba/mamba}"

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

check_dwd_pkg() {
    if [ -e "${test_dir}/pkgs/test-package-0.1-0.tar.bz2" ]; then
        echo -e "\e[32mtest-package-0.1-0.tar.bz2 successfully (verified) and downloaded!\e[0m"
    else
        echo -e "\e[31mtest-package-0.1-0.tar.bz2 does not exist!\e[0m"
        exit 1
    fi
}

test_install() {
	local tmp=$(mktemp -d)
	local condarc="${tmp}/condarc"
	"${TEST_MAMBA_EXE}" create -y -p "${tmp}/env1" --override-channels -c $1/mychannel test-package -vvv --repodata-ttl=0 "${@:2}"
	check_dwd_pkg
	"${TEST_MAMBA_EXE}" clean --all -f -y
}

# Test without authentication
start_server & PID=$!
test_install http://localhost:8000
kill -TERM $PID

# Tests with authentication
start_server --auth basic --user user --password test & PID=$!
test_install http://user:test@localhost:8000
kill -TERM $PID

start_server --auth basic --user user@email.com --password test & PID=$!
test_install http://user%40email.com:test@localhost:8000
kill -TERM $PID

start_server --token xy-12345678-1234-1234-1234-123456789012 & PID=$!
test_install http://localhost:8000/t/xy-12345678-1234-1234-1234-123456789012
kill -TERM $PID

# Verify signed packages
if [[ "$(uname -s)" == "Linux" ]]; then
	export KEY1=$(gpg --fingerprint "MAMBA1")
	export KEY2=$(gpg --fingerprint "MAMBA2")

	start_server --auth none --sign & PID=$!
	test_install http://localhost:8000 --trusted-channels "http://localhost:8000/mychannel" --verify-artifacts
	kill -TERM $PID
fi
