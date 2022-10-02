#!/bin/bash

set -exo pipefail

# Don't test cross-compiled result (there is no emulation) and use the latest MacOS SDK.
if grep -q "osx-arm64" .ci_support/${CONDA_BUILD_YML}.yaml; then
  CONDA_BUILD_ARGS="--no-test"
fi
conda mambabuild -m .ci_support/${CONDA_BUILD_YML}.yaml conda.recipe ${CONDA_BUILD_ARGS:-}
