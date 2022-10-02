#!/bin/bash

set -eo pipefail

export CONDA_BUILD_YML=$1

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source ${SCRIPT_DIR}/base.sh $*
conda activate base
mamba install -y conda-build
conda build -m .ci_support/${CONDA_BUILD_YML}.yaml conda.recipe
