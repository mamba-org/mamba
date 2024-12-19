# Copied from conda.sh which is licensed as below
#
# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause

__mamba_exe() (
    \local MAMBA_CONDA_EXE_BACKUP=$CONDA_EXE
    \local MAMBA_EXE=$(\dirname "${CONDA_EXE}")/mamba
    "$MAMBA_EXE" $_CE_M $_CE_CONDA "$@"
)

mamba() {
    \local cmd="${1-__missing__}"
    case "$cmd" in
        activate|deactivate)
            __conda_activate "$@"
            ;;
        install|update|upgrade|remove|uninstall)
            __mamba_exe "$@" || \return
            __conda_activate reactivate
            ;;
        *)
            __mamba_exe "$@"
            ;;
    esac
}
