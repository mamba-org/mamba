# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause

__mamba_exe() (
    "${MAMBA_EXE}" "${@}"
)

__mamba_hashr() {
    if [ -n "${ZSH_VERSION:+x}" ]; then
        \rehash
    elif [ -n "${POSH_VERSION:+x}" ]; then
        :  # pass
    else
        \hash -r
    fi
}

__mamba_xctivate() {
    \local ask_mamba
    ask_mamba="$(PS1="${PS1:-}" __mamba_exe shell "${@}" --shell bash)" || \return
    \eval "${ask_mamba}"
    __mamba_hashr
}

__mamba_wrap() {
    \local cmd="${1-__missing__}"
    case "${cmd}" in
        activate|reactivate|deactivate)
            __mamba_xctivate "${@}"
            ;;
        install|update|upgrade|remove|uninstall)
            __mamba_exe "${@}" || \return
            __mamba_xctivate reactivate
            ;;
        self-update)
            __mamba_exe "${@}" || \return

            # remove leftover backup file on Windows
            if [ -f "${MAMBA_EXE}.bkup" ]; then
                rm -f "${MAMBA_EXE}.bkup"
            fi
            ;;
        *)
            __mamba_exe "${@}"
            ;;
    esac
}


# We need to define a function with the same name as the executable to be called by the user.
# There is no way to register it dynamically without relying on hacks or eval.
__exe_name="$(basename "${MAMBA_EXE}")"
__exe_name="${__exe_name%.*}"

case "${__exe_name}" in
    micromamba)
        micromamba() {
            __mamba_wrap "$@"
        }
        ;;
    mamba)
        mamba() {
            __mamba_wrap "$@"
        }
        ;;
    *)
        echo "Error unknown MAMBA_EXE: \"${MAMBA_EXE}\", filename must be mamba or micromamba" 1>&2
        ;;
esac


if [ -z "${CONDA_SHLVL+x}" ]; then
    \export CONDA_SHLVL=0
    \export PATH="${MAMBA_ROOT_PREFIX}/condabin:${PATH}"

    # We're not allowing PS1 to be unbound. It must at least be set.
    # However, we're not exporting it, which can cause problems when starting a second shell
    # via a first shell (i.e. starting zsh from bash).
    if [ -z "${PS1+x}" ]; then
        PS1=
    fi
fi
