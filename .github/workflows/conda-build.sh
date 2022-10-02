#!/bin/bash

set -eo pipefail

# add --no-test if we are not building for x86
if [[ $CONDA_BUILD_YML != linux_64_ ]]; then
  CONDA_BUILD_ARGS="--no-test"
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source ${SCRIPT_DIR}/base.sh $*
conda activate base
mamba install -y conda-build
conda build -m .ci_support/${CONDA_BUILD_YML}.yaml conda.recipe ${CONDA_BUILD_ARGS:-}
