# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause

if ("`alias conda`" == "") then
    alias conda source "${MAMBA_ROOT_PREFIX}/conda/shell/etc/profile.d/micromamba.csh"
    setenv CONDA_SHLVL 0
    if (! $?prompt) then
        set prompt=""
    endif
else
    switch ( "${1}" )
        case "activate":
            set ask_conda="`(setenv prompt '${prompt}' ; '${MAMBA_EXE}' -s csh activate '${2}' ${argv[3-]})`"
            set conda_tmp_status=$status
            if( $conda_tmp_status != 0 ) exit ${conda_tmp_status}
            eval "${ask_conda}"
            rehash
            breaksw
        case "deactivate":
            set ask_conda="`(setenv prompt '${prompt}' ; '${MAMBA_EXE}' -s csh deactivate '${2}' ${argv[3-]})`"
            set conda_tmp_status=$status
            if( $conda_tmp_status != 0 ) exit ${conda_tmp_status}
            eval "${ask_conda}"
            rehash
            breaksw
        case "install" | "update" | "upgrade" | "remove" | "uninstall":
            $MAMBA_EXE $argv[1-]
            set ask_conda="`(setenv prompt '${prompt}' ; '${MAMBA_EXE}' -s csh reactivate)`"
            set conda_tmp_status=$status
            if( $conda_tmp_status != 0 ) exit ${conda_tmp_status}
            eval "${ask_conda}"
            rehash
            breaksw
        default:
            $MAMBA_EXE $argv[1-]
            breaksw
    endsw
endif
