# -*- coding: utf-8 -*-
# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

from __future__ import absolute_import, division, print_function, unicode_literals

import sys, os

from os.path import abspath, basename, exists, isdir, isfile, join

from conda.cli.main import generate_parser, init_loggers
from conda.base.context import context
from conda.core.index import calculate_channel_urls, check_whitelist #, get_index
from conda.models.channel import Channel, prioritize_channels
from conda.models.records import PackageRecord
from conda.models.match_spec import MatchSpec
from conda.cli.main_list import list_packages
from conda.core.prefix_data import PrefixData
from conda.misc import clone_env, explicit, touch_nonadmin
from conda.common.serialize import json_dump
from conda.cli.common import specs_from_args, specs_from_url, confirm_yn, check_non_admin, ensure_name_or_prefix
from conda.core.subdir_data import SubdirData
from conda.core.link import UnlinkLinkTransaction, PrefixSetup
from conda.cli.install import handle_txn, check_prefix, clone, print_activate
from conda.base.constants import ChannelPriority, ROOT_ENV_NAME, UpdateModifier
from conda.core.solve import diff_for_unlink_link_precs
# create support
from conda.common.path import paths_equal
from conda.exceptions import (CondaExitZero, CondaImportError, CondaOSError, CondaSystemExit,
                              CondaValueError, DirectoryNotACondaEnvironmentError,
                              DirectoryNotFoundError, DryRunExit, EnvironmentLocationNotFound,
                              NoBaseEnvironmentError, PackageNotInstalledError, PackagesNotFoundError,
                              TooManyArgumentsError, UnsatisfiableError)

from conda.gateways.disk.create import mkdir_p
from conda.gateways.disk.delete import rm_rf, delete_trash, path_is_clean
from conda.gateways.disk.test import is_conda_environment

from conda._vendor.boltons.setutils import IndexedSet
from conda.models.prefix_graph import PrefixGraph

from logging import getLogger
from os.path import isdir

import json
import tempfile

import mamba.mamba_api as api

from mamba.utils import get_index, get_channel, to_package_record_from_subjson

log = getLogger(__name__)
stderrlog = getLogger('mamba.stderr')

banner = """
                  __    __    __    __
                 /  \\  /  \\  /  \\  /  \\
                /    \\/    \\/    \\/    \\
███████████████/  /██/  /██/  /██/  /████████████████████████
              /  / \\   / \\   / \\   / \\  \\____
             /  /   \\_/   \\_/   \\_/   \\    o \\__,
            / _/                       \\_____/  `
            |/
        ███╗   ███╗ █████╗ ███╗   ███╗██████╗  █████╗ 
        ████╗ ████║██╔══██╗████╗ ████║██╔══██╗██╔══██╗
        ██╔████╔██║███████║██╔████╔██║██████╔╝███████║
        ██║╚██╔╝██║██╔══██║██║╚██╔╝██║██╔══██╗██╔══██║
        ██║ ╚═╝ ██║██║  ██║██║ ╚═╝ ██║██████╔╝██║  ██║
        ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝     ╚═╝╚═════╝ ╚═╝  ╚═╝
                          
        Supported by @QuantStack
    
        Github:  https://github.com/QuantStack/mamba
        Twitter: https://twitter.com/QuantStack

█████████████████████████████████████████████████████████████
"""

def get_installed_packages(prefix, show_channel_urls=None):
    result = {'packages': {}}

    # Currently, we need to have pip interop disabled :/
    installed = list(PrefixData(prefix, pip_interop_enabled=False).iter_records())

    for prec in installed:
        json_rec = prec.dist_fields_dump()
        json_rec['depends'] = prec.depends
        json_rec['build'] = prec.build
        result['packages'][prec.fn] = json_rec

    return installed, result

def install(args, parser, command='install'):
    """
    mamba install, mamba update, and mamba create
    """
    context.validate_configuration()
    check_non_admin()

    newenv = bool(command == 'create')
    isupdate = bool(command == 'update')
    isinstall = bool(command == 'install')
    if newenv:
        ensure_name_or_prefix(args, command)
    prefix = context.target_prefix
    if newenv:
        check_prefix(prefix, json=context.json)
    if context.force_32bit and prefix == context.root_prefix:
        raise CondaValueError("cannot use CONDA_FORCE_32BIT=1 in base env")
    if isupdate and not (args.file or args.packages
                         or context.update_modifier == UpdateModifier.UPDATE_ALL):
        raise CondaValueError("""no package names supplied
# If you want to update to a newer version of Anaconda, type:
#
# $ conda update --prefix %s anaconda
""" % prefix)

    if not newenv:
        if isdir(prefix):
            delete_trash(prefix)
            if not isfile(join(prefix, 'conda-meta', 'history')):
                if paths_equal(prefix, context.conda_prefix):
                    raise NoBaseEnvironmentError()
                else:
                    if not path_is_clean(prefix):
                        raise DirectoryNotACondaEnvironmentError(prefix)
            else:
                # fall-through expected under normal operation
                pass
        else:
            if args.mkdir:
                try:
                    mkdir_p(prefix)
                except EnvironmentError as e:
                    raise CondaOSError("Could not create directory: %s" % prefix, caused_by=e)
            else:
                raise EnvironmentLocationNotFound(prefix)

    # context.__init__(argparse_args=args)

    prepend = not args.override_channels
    prefix = context.target_prefix

    index_args = {
        'use_cache': args.use_index_cache,
        'channel_urls': context.channels,
        'unknown': args.unknown,
        'prepend': not args.override_channels,
        'use_local': args.use_local
    }

    args_packages = [s.strip('"\'') for s in args.packages]
    if newenv and not args.no_default_packages:
        # Override defaults if they are specified at the command line
        # TODO: rework in 4.4 branch using MatchSpec
        args_packages_names = [pkg.replace(' ', '=').split('=', 1)[0] for pkg in args_packages]
        for default_pkg in context.create_default_packages:
            default_pkg_name = default_pkg.replace(' ', '=').split('=', 1)[0]
            if default_pkg_name not in args_packages_names:
                args_packages.append(default_pkg)

    num_cp = sum(s.endswith('.tar.bz2') for s in args_packages)
    if num_cp:
        if num_cp == len(args_packages):
            explicit(args_packages, prefix, verbose=not context.quiet)
            return
        else:
            raise CondaValueError("cannot mix specifications with conda package"
                                  " filenames")

    index = get_index(channel_urls=index_args['channel_urls'],
                      prepend=index_args['prepend'], platform=None,
                      use_local=index_args['use_local'], use_cache=index_args['use_cache'],
                      unknown=index_args['unknown'], prefix=prefix)

    channel_json = []
    for x in index:
        # add priority here
        if x.channel.name in index_args['channel_urls']:
            priority = len(index_args['channel_urls']) - index_args['channel_urls'].index(x.channel.name)
        else:
            priority = 0
        channel_json.append((str(x.channel), x.cache_path_json, priority))

    installed_pkg_recs, output = get_installed_packages(prefix, show_channel_urls=True)
    installed_json_f = tempfile.NamedTemporaryFile('w', delete=False)
    installed_json_f.write(json_dump(output))
    installed_json_f.flush()

    specs = []
    if args.file:
        for fpath in args.file:
            try:
                specs.extend(specs_from_url(fpath, json=context.json))
            except Unicode:
                raise CondaError("Error reading file, file should be a text file containing"
                                 " packages \nconda create --help for details")
        if '@EXPLICIT' in specs:
            explicit(specs, prefix, verbose=not context.quiet, index_args=index_args)
            return

    specs.extend(specs_from_args(args_packages, json=context.json))

    if isinstall and args.revision:
        get_revision(args.revision, json=context.json)
    elif isinstall and not (args.file or args_packages):
        raise CondaValueError("too few arguments, "
                              "must supply command line package specs or --file")

    # for 'conda update', make sure the requested specs actually exist in the prefix
    # and that they are name-only specs
    if isupdate and context.update_modifier == UpdateModifier.UPDATE_ALL:
        print("Currently, mamba can only update explicit packages! (e.g. mamba update numpy python ...)")
        exit()

    if isupdate and context.update_modifier != UpdateModifier.UPDATE_ALL:
        prefix_data = PrefixData(prefix)
        for spec in specs:
            spec = MatchSpec(spec)
            if not spec.is_name_only_spec:
                raise CondaError("Invalid spec for 'conda update': %s\n"
                                 "Use 'conda install' instead." % spec)
            if not prefix_data.get(spec.name, None):
                raise PackageNotInstalledError(prefix, spec.name)

    if newenv and args.clone:
        if args.packages:
            raise TooManyArgumentsError(0, len(args.packages), list(args.packages),
                                        'did not expect any arguments for --clone')

        clone(args.clone, prefix, json=context.json, quiet=context.quiet, index_args=index_args)
        touch_nonadmin(prefix)
        print_activate(args.name if args.name else prefix)
        return

    specs = [MatchSpec(s) for s in specs]
    mamba_solve_specs = [s.conda_build_form() for s in specs]

    print("\n\nLooking for: {}\n\n".format(mamba_solve_specs))


    strict_priority = (context.channel_priority == ChannelPriority.STRICT)
    if strict_priority:
        raise Exception("Cannot use strict priority with mamba!")

    to_link, to_unlink = api.solve(channel_json, installed_json_f.name, mamba_solve_specs, isupdate, strict_priority)

    to_link_records, to_unlink_records = [], []

    final_precs = IndexedSet(PrefixData(prefix).iter_records())

    def get_channel(c):
        for x in index:
            if str(x.channel) == c:
                return x

    for c, pkg in to_unlink:
        for i_rec in installed_pkg_recs:
            if i_rec.fn == pkg:
                final_precs.remove(i_rec)
                to_unlink_records.append(i_rec)
                break
        else:
            print("No package record found!")

    for c, pkg, jsn_s in to_link:
        sdir = get_channel(c)
        rec = to_package_record_from_subjson(sdir, pkg, jsn_s)
        final_precs.add(rec)
        to_link_records.append(rec)

    unlink_precs, link_precs = diff_for_unlink_link_precs(prefix,
                                                          final_precs=IndexedSet(PrefixGraph(final_precs).graph),
                                                          specs_to_add=specs,
                                                          force_reinstall=context.force_reinstall)

    pref_setup = PrefixSetup(
        target_prefix = prefix,
        unlink_precs  = unlink_precs,
        link_precs    = link_precs,
        remove_specs  = [],
        update_specs  = specs
    )

    conda_transaction = UnlinkLinkTransaction(pref_setup)
    handle_txn(conda_transaction, prefix, args, newenv)

    try:
        installed_json_f.close()
        os.unlink(installed_json_f.name)
    except:
        pass

def create(args, parser):
    if is_conda_environment(context.target_prefix):
        if paths_equal(context.target_prefix, context.root_prefix):
            raise CondaValueError("The target prefix is the base prefix. Aborting.")
        confirm_yn("WARNING: A conda environment already exists at '%s'\n"
                   "Remove existing environment" % context.target_prefix,
                   default='no',
                   dry_run=False)
        log.info("Removing existing environment %s", context.target_prefix)
        rm_rf(context.target_prefix)
    elif isdir(context.target_prefix):
        confirm_yn("WARNING: A directory already exists at the target location '%s'\n"
                   "but it is not a conda environment.\n"
                   "Continue creating environment" % context.target_prefix,
                   default='no',
                   dry_run=False)
    install(args, parser, 'create')

def update(args, parser):
    if context.force:
        print("\n\n"
              "WARNING: The --force flag will be removed in a future conda release.\n"
              "         See 'conda update --help' for details about the --force-reinstall\n"
              "         and --clobber flags.\n"
              "\n", file=sys.stderr)

    # need to implement some modifications on the update function
    install(args, parser, 'update')


def do_call(args, parser):
    relative_mod, func_name = args.func.rsplit('.', 1)
    # func_name should always be 'execute'
    if relative_mod in ['.main_list', '.main_search', '.main_run', '.main_clean', '.main_info']:
        from importlib import import_module
        module = import_module('conda.cli' + relative_mod, __name__.rsplit('.', 1)[0])
        exit_code = getattr(module, func_name)(args, parser)
    elif relative_mod == '.main_install':
        exit_code = install(args, parser, 'install')
    elif relative_mod == '.main_create':
        exit_code = create(args, parser)
    elif relative_mod == '.main_update':
        exit_code = update(args, parser)
    else:
        print("Currently, only install, create, list, search, run, info and clean are supported through mamba.")
        return 0
    return exit_code

def _wrapped_main(*args, **kwargs):
    if len(args) == 1:
        args = args + ('-h',)

    p = generate_parser()
    args = p.parse_args(args[1:])

    context.__init__(argparse_args=args)
    init_loggers(context)

    # from .conda_argparse import do_call
    exit_code = do_call(args, p)
    if isinstance(exit_code, int):
        return exit_code

# Main entry point!
def main(*args, **kwargs):
    # Set to false so we don't allow uploading our issues to conda!
    context.report_errors = False

    from conda.common.compat import ensure_text_type, init_std_stream_encoding
    init_std_stream_encoding()

    print(banner)

    if 'activate' in sys.argv or 'deactivate' in sys.argv:
        print("Use conda to activate / deactivate the environment.")
        print('\n    $ conda ' + ' '.join(sys.argv[1:]) + '\n')
        return sys.exit(-1)

    if not args:
        args = sys.argv

    args = tuple(ensure_text_type(s) for s in args)

    if args[1] == 'env' and args[2] == 'create':
        # special handling for conda env create!
        from mamba import mamba_env
        return mamba_env.main()

    from conda.exceptions import conda_exception_handler
    return conda_exception_handler(_wrapped_main, *args, **kwargs)
