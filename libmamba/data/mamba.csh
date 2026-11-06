# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause

# csh aliases cannot hold multi-line logic, so the alias sources this file again
# with "__mamba_wrap" followed by the command line arguments.
set __mamba_status = 0
set __mamba_wrap_mode = 0
if ("$argv[1-]" =~ "__mamba_wrap*") then
    set __mamba_wrap_mode = 1
endif

if ($__mamba_wrap_mode == 0) then
    set __exe_name = "`basename '$MAMBA_EXE'`"
    if ("$__exe_name" == "micromamba" || "$__exe_name" == "mamba") then
        # Without history references, the alias appends its arguments.
        alias "$__exe_name" "source '${MAMBA_ROOT_PREFIX}/etc/profile.d/mamba.csh' __mamba_wrap"
    else
        echo "Error unknown MAMBA_EXE: '$MAMBA_EXE', filename must be mamba or micromamba" > /dev/stderr
    endif
    unset __exe_name

    if (! $?CONDA_SHLVL) then
        setenv CONDA_SHLVL 0
        setenv PATH "${MAMBA_ROOT_PREFIX}/condabin:${PATH}"

        # We're not allowing PS1 to be unbound. It must at least be set.
        if (! $?PS1) then
            setenv PS1 ''
        endif
    endif
else
    shift
    set __mamba_cmd = ""
    if ($#argv > 0) then
        set __mamba_cmd = "$argv[1]"
    endif
    set __mamba_shell_args = ()
    switch ("$__mamba_cmd")
        case activate:
        case reactivate:
        case deactivate:
            set __mamba_shell_args = ($argv:q)
            breaksw
        case install:
        case update:
        case upgrade:
        case remove:
        case uninstall:
            "$MAMBA_EXE" $argv:q
            set __mamba_status = $status
            if ($__mamba_status == 0) then
                set __mamba_shell_args = (reactivate)
            endif
            breaksw
        case self-update:
            "$MAMBA_EXE" $argv:q
            set __mamba_status = $status
            if ($__mamba_status == 0 && -f "$MAMBA_EXE.bkup") then
                rm -f "$MAMBA_EXE.bkup"
            endif
            breaksw
        default:
            "$MAMBA_EXE" $argv:q
            set __mamba_status = $status
            breaksw
    endsw

    # Activation commands print a script that must be sourced by the current shell.
    if ($#__mamba_shell_args > 0) then
        set __mamba_script = "`mktemp`"
        if ($?prompt) then
            setenv prompt "$prompt:q"
        endif
        "$MAMBA_EXE" shell $__mamba_shell_args:q --shell csh >! "$__mamba_script"
        set __mamba_status = $status
        unsetenv prompt
        if ($__mamba_status == 0) then
            source "$__mamba_script"
            rehash
        endif
        rm -f "$__mamba_script"
        unset __mamba_script
    endif
    unset __mamba_cmd __mamba_shell_args
endif
unset __mamba_wrap_mode
# Propagate the exit status of the wrapped command to the caller.
# This must stay the last line: closing a block resets the status.
eval "unset __mamba_status; (exit $__mamba_status)"
