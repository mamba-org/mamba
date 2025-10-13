# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause

alias __mamba_exe '"$MAMBA_EXE" "\!*"'

alias __mamba_hashr 'rehash'

alias __mamba_xctivate '\\
    set ask_conda="`(setenv prompt "${prompt}"; __mamba_exe shell "\!*" --shell csh)`"\\
    if ("${status}" != 0) then\\
        return\\
     endif\\
         eval "${ask_conda}"\\
     __mamba_hashr\\
'

alias __mamba_wrap '\\
    switch ("${1}")\\
        case activate | reactivate | deactivate:\\
            __mamba_xctivate "\!*"\\
            breaksw\\
        case install | update | upgrade | remove | uninstall:\\
            __mamba_exe "\!*"\\
            if ("${status}" != 0) then\\
                return\\
             endif\\
            __mamba_xctivate reactivate\\
            breaksw\\
        case self-update:\\
            __mamba_exe "\!*"\\
            if ("${status}" != 0) then\\
                return\\
             endif\\
             if (-f "$MAMBA_EXE.bkup") then\\
                rm -f "$MAMBA_EXE.bkup"\\
             endif\\
            breaksw\\
        default:\\
            __mamba_exe "\!*"\\
            breaksw\\
    endsw\\
'

set __exe_name=(`basename $MAMBA_EXE`)
set __exe_name = (${__exe_name}:q.)
if ("$__exe_name" == "micromamba") then
    alias mamba __mamba_wrap
else if ("$__exe_name" == "mamba") then
    alias micromamba __mamba_wrap
else
    echo "Error unknown MAMBA_EXE: \"$MAMBA_EXE\", filename must be mamba or micromamba" >&2
endif

if (! $?CONDA_SHLVL) then
    setenv CONDA_SHLVL 0
    # In dev-mode MAMBA_EXE is python.exe and on Windows
    # it is in a different relative location to condabin.
    if ($?_CE_CONDA && $?WINDIR) then
        setenv PATH "${MAMBA_ROOT_PREFIX}/condabin:${PATH}"
    else
        setenv PATH "${MAMBA_ROOT_PREFIX}/condabin:${PATH}"
    endif

    # We're not allowing PS1 to be unbound. It must at least be set.
    # However, we're not exporting it, which can cause problems when starting a second shell
    # via a first shell (i.e. starting zsh from bash).
    if (! $?PS1) then
        setenv PS1 ''
    endif
endif
