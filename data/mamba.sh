R"MAMBARAW(
# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause

__add_sys_prefix_to_path() {
    # In dev-mode MAMBA_EXE is python.exe and on Windows
    # it is in a different relative location to condabin.
    if [ -z "${MAMBA_ROOT_PREFIX}" ]; then
        return 0
    fi;

    if [ -n "${WINDIR+x}" ]; then
        PATH="${SYSP}/bin:${PATH}"
        PATH="${SYSP}/Scripts:${PATH}"
        PATH="${SYSP}/Library/bin:${PATH}"
        PATH="${SYSP}/Library/usr/bin:${PATH}"
        PATH="${SYSP}/Library/mingw-w64/bin:${PATH}"
        PATH="${SYSP}:${PATH}"
    else
        PATH="${SYSP}/bin:${PATH}"
    fi
    \export PATH
}


__conda_hashr() {
    if [ -n "${ZSH_VERSION:+x}" ]; then
        \rehash
    elif [ -n "${POSH_VERSION:+x}" ]; then
        :  # pass
    else
        \hash -r
    fi
}

__mamba_activate() {
    \local cmd="$1"
    shift
    \local ask_conda
    CONDA_INTERNAL_OLDPATH="${PATH}"
    __add_sys_prefix_to_path
    \local prefix="$@"
    if [ "$prefix" = "" ]; then
        prefix="base"
    fi
    ask_conda="$(PS1="$PS1" "$MAMBA_EXE" shell --shell bash "$cmd" --prefix "$prefix")" || \return $?
    rc=$?
    PATH="${CONDA_INTERNAL_OLDPATH}"
    \eval "$ask_conda"
    if [ $rc != 0 ]; then
        \export PATH
    fi
    __conda_hashr
}

__mamba_reactivate() {
    \local ask_conda
    CONDA_INTERNAL_OLDPATH="${PATH}"
    __add_sys_prefix_to_path
    ask_conda="$(PS1="$PS1" "$MAMBA_EXE" shell --shell bash reactivate)" || \return $?
    PATH="${CONDA_INTERNAL_OLDPATH}"
    \eval "$ask_conda"
    __conda_hashr
}

micromamba() {
    if [ "$#" -lt 1 ]; then
        "$MAMBA_EXE"
    else
        \local cmd="$1"
        shift
        case "$cmd" in
            activate|deactivate)
                __mamba_activate "$cmd" "$@"
                ;;
            install|update|upgrade|remove|uninstall)
                CONDA_INTERNAL_OLDPATH="${PATH}"
                __add_sys_prefix_to_path
                "$MAMBA_EXE" "$cmd" "$@"
                \local t1=$?
                PATH="${CONDA_INTERNAL_OLDPATH}"
                if [ $t1 = 0 ]; then
                    __mamba_reactivate
                else
                    return $t1
                fi
                ;;
            *)
                CONDA_INTERNAL_OLDPATH="${PATH}"
                __add_sys_prefix_to_path
                "$MAMBA_EXE" "$cmd" "$@"
                \local t1=$?
                PATH="${CONDA_INTERNAL_OLDPATH}"
                return $t1
                ;;
        esac
    fi
}

if [ -z "${CONDA_SHLVL+x}" ]; then
    \export CONDA_SHLVL=0
    # In dev-mode MAMBA_EXE is python.exe and on Windows
    # it is in a different relative location to condabin.
    if [ -n "${_CE_CONDA+x}" ] && [ -n "${WINDIR+x}" ]; then
        PATH="$(\dirname "$MAMBA_ROOT_PREFIX")/condabin${PATH:+":${PATH}"}"
    else
        PATH="$(\dirname "$(\dirname "$MAMBA_ROOT_PREFIX")")/condabin${PATH:+":${PATH}"}"
    fi
    \export PATH

    # We're not allowing PS1 to be unbound. It must at least be set.
    # However, we're not exporting it, which can cause problems when starting a second shell
    # via a first shell (i.e. starting zsh from bash).
    if [ -z "${PS1+x}" ]; then
        PS1=
    fi
fi
)MAMBARAW"
