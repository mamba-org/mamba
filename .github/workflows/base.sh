#!/bin/bash

set -eo pipefail

export PYTHON_VERSION=$1

# Enable 'conda' command to make 'conda activate' work
CONDA_BASE=$(conda info --base)
source ${CONDA_BASE}/etc/profile.d/conda.sh
