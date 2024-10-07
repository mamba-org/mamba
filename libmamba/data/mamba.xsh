# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause
# Much of this forked from https://github.com/gforsyth/xonda
# Copyright (c) 2016, Gil Forsyth, All rights reserved.
# Original code licensed under BSD-3-Clause.

try:
    # xonsh >= 0.18.0
    from xonsh.lib.lazyasd import lazyobject
except:
    # xonsh < 0.18.0
    from xonsh.lazyasd import lazyobject

from xonsh.completers import completer
from xonsh.completers.tools import complete_from_sub_proc, contextual_command_completer

_REACTIVATE_COMMANDS = ('install', 'update', 'upgrade', 'remove', 'uninstall')


def _parse_args(args=None):
    from argparse import ArgumentParser
    p = ArgumentParser(add_help=False)
    p.add_argument('command')
    ns, _ = p.parse_known_args(args)
    if ns.command == 'activate':
        p.add_argument('env_name_or_prefix', default='base')
    elif ns.command in _REACTIVATE_COMMANDS:
        p.add_argument('-n', '--name')
        p.add_argument('-p', '--prefix')
    parsed_args, _ = p.parse_known_args(args)
    return parsed_args


def _raise_pipeline_error(pipeline):
    stdout = pipeline.out
    stderr = pipeline.err
    if pipeline.returncode != 0:
        message = ("exited with %s\nstdout: %s\nstderr: %s\n"
                   "" % (pipeline.returncode, stdout, stderr))
        raise RuntimeError(message)
    return stdout.strip()


def _mamba_activate_handler(env_name_or_prefix=None):
    if env_name_or_prefix == 'base' or not env_name_or_prefix:
        env_name_or_prefix = $MAMBA_ROOT_PREFIX
    __xonsh__.execer.exec($($MAMBA_EXE shell activate -s xonsh -p @(env_name_or_prefix)),
                          glbs=__xonsh__.ctx,
                          filename="$($MAMBA_EXE shell activate -s xonsh -p " + env_name_or_prefix + ")")


def _mamba_deactivate_handler():
    __xonsh__.execer.exec($($MAMBA_EXE shell deactivate -s xonsh),
                          glbs=__xonsh__.ctx,
                          filename="$($MAMBA_EXE shell deactivate -s xonsh)")


def _mamba_passthrough_handler(args):
    pipeline = ![$MAMBA_EXE @(args)]
    _raise_pipeline_error(pipeline)


def _mamba_reactivate_handler(args, name_or_prefix_given):
    pipeline = ![$MAMBA_EXE @(args)]
    _raise_pipeline_error(pipeline)
    if not name_or_prefix_given:
        __xonsh__.execer.exec($($MAMBA_EXE shell reactivate -s xonsh),
                              glbs=__xonsh__.ctx,
                              filename="$($MAMBA_EXE shell -s xonsh reactivate)")


def _micromamba_main(args=None):
    parsed_args = _parse_args(args)
    if parsed_args.command == 'activate':
        _mamba_activate_handler(parsed_args.env_name_or_prefix)
    elif parsed_args.command == 'deactivate':
        _mamba_deactivate_handler()
    elif parsed_args.command in _REACTIVATE_COMMANDS:
        name_or_prefix_given = bool(parsed_args.name or parsed_args.prefix)
        _mamba_reactivate_handler(args, name_or_prefix_given)
    else:
        _mamba_passthrough_handler(args)


if 'CONDA_SHLVL' not in ${...}:
    $CONDA_SHLVL = '0'
    import os as _os
    import sys as _sys
    _sys.path.insert(0, _os.path.join($MAMBA_ROOT_PREFIX, "condabin"))
    del _os, _sys


aliases['micromamba'] = _micromamba_main


@contextual_command_completer
def _micromamba_proc_completer(ctx):
    if not ctx.args:
        return

    return (
        complete_from_sub_proc(
            "micromamba",
            "completer",
            *[a.value for a in ctx.args[1:]],
            ctx.prefix,
            sep=lambda x: x.split()
        ),
        False,
    )

completer.add_one_completer("micromamba", _micromamba_proc_completer, "<bash")
