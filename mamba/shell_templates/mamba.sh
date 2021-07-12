# Copied from conda.sh which is licensed as below
#
# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause

mamba() {
    \local MAMBA_CONDA_EXE_BACKUP=$CONDA_EXE
    \local MAMBA_EXE=$(\dirname "${CONDA_EXE}")/mamba
    if [ "$#" -lt 1 ]; then
        "$MAMBA_EXE" $_CE_M $_CE_CONDA
    else
        \local cmd="$1"
        shift
        case "$cmd" in
            activate|deactivate)
                __conda_activate "$cmd" "$@"
                ;;
            install|update|upgrade|remove|uninstall)
                CONDA_INTERNAL_OLDPATH="${PATH}"
                __add_sys_prefix_to_path
                "$MAMBA_EXE" $_CE_M $_CE_CONDA "$cmd" "$@"
                \local t1=$?
                PATH="${CONDA_INTERNAL_OLDPATH}"
                if [ $t1 = 0 ]; then
                    __conda_reactivate
                else
                    return $t1
                fi
                ;;
            *)
                CONDA_INTERNAL_OLDPATH="${PATH}"
                __add_sys_prefix_to_path
                "$MAMBA_EXE" $_CE_M $_CE_CONDA "$cmd" "$@"
                \local t1=$?
                PATH="${CONDA_INTERNAL_OLDPATH}"
                return $t1
                ;;
        esac
    fi
}
