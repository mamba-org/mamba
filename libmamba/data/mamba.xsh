# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause
# Much of this forked from https://github.com/gforsyth/xonda
# Copyright (c) 2016, Gil Forsyth, All rights reserved.
# Original code licensed under BSD-3-Clause.
from xonsh.lazyasd import lazyobject

_REACTIVATE_COMMANDS = ('install', 'update', 'upgrade', 'remove', 'uninstall')

@lazyobject
def Env():
    from collections import namedtuple
    return namedtuple('Env', ['name', 'path', 'bin_dir', 'envs_dir'])


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


def _mamba_activate_handler(env_name_or_prefix):
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


def _list_dirs(path):
    """
    Generator that lists the directories in a given path.
    """
    import os
    for entry in os.scandir(path):
        if not entry.name.startswith('.') and entry.is_dir():
            yield entry.name


def _mamba_completer(prefix, line, start, end, ctx):
    """
    Completion for conda
    """
    args = line.split(' ')
    possible = set()
    if len(args) == 0 or args[0] not in ['micromamba']:
        return None
    curix = args.index(prefix)
    if curix == 1:
        possible = {'activate', 'deactivate', 'install', 'create'}

    elif curix == 2:
        if args[1] == 'create':
            possible = {'-p'}

    return {i for i in possible if i.startswith(prefix)}


# add _xonda_completer to list of completers
__xonsh__.completers['micromamba'] = _mamba_completer
# bump to top of list
__xonsh__.completers.move_to_end('micromamba', last=False)
