# -*- coding: utf-8 -*-
# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause
import sys, os

from conda.cli.main import generate_parser, init_loggers
from conda.base.context import context
from conda.core.index import calculate_channel_urls, check_whitelist #, get_index
from conda.models.channel import Channel, prioritize_channels
from conda.models.records import PackageRecord
from conda.models.match_spec import MatchSpec
from conda.cli.main_list import list_packages
from conda.core.prefix_data import PrefixData
from conda.common.serialize import json_dump
from conda.cli.common import specs_from_args, specs_from_url, confirm_yn
from conda.core.subdir_data import SubdirData
from conda.common.url import join_url
from conda.core.link import UnlinkLinkTransaction, PrefixSetup
from conda.cli.install import handle_txn
from conda.base.constants import ChannelPriority

# create support
from conda.common.path import paths_equal
from conda.exceptions import CondaValueError
from conda.gateways.disk.delete import rm_rf
from conda.gateways.disk.test import is_conda_environment

from logging import getLogger
from os.path import isdir

import json
import tempfile
from multiprocessing.pool import Pool as MPool

from .FastSubdirData import FastSubdirData
import mamba.mamba_api as api

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



def get_channel(x):
    print("Getting ", x)
    return FastSubdirData(Channel(x)).load()

def get_index(channel_urls=(), prepend=True, platform=None,
              use_local=False, use_cache=False, unknown=None, prefix=None):
    channel_urls = calculate_channel_urls(channel_urls, prepend, platform, use_local)
    check_whitelist(channel_urls)
    pl = MPool(8)
    result = pl.map(get_channel, channel_urls)
    return result

def to_package_record_from_subjson(subdir, pkg, jsn_string):
    channel = subdir.channel
    channel_url = subdir.url_w_credentials
    info = json.loads(jsn_string)
    info['fn'] = pkg
    info['channel'] = channel
    info['url'] = join_url(channel_url, pkg)
    package_record = PackageRecord(**info)
    return package_record

    # if add_pip and info['name'] == 'python' and info['version'].startswith(('2.', '3.')):
    #     info['depends'].append('pip')

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

def install(args, parser, function):
    context.__init__(argparse_args=args)

    prepend = not args.override_channels
    prefix = context.target_prefix

    index_args = {
        'use_cache': args.use_index_cache,
        'channel_urls': context.channels,
        'unknown': args.unknown,
        'prepend': not args.override_channels,
        'use_local': args.use_local
    }

    index = get_index(channel_urls=index_args['channel_urls'],
                      prepend=index_args['prepend'], platform=None,
                      use_local=index_args['use_local'], use_cache=index_args['use_cache'],
                      unknown=index_args['unknown'], prefix=prefix)

    channel_json = [(str(x.channel), x.cache_path_json) for x in index]

    installed_pkg_recs, output = get_installed_packages(prefix, show_channel_urls=True)
    installed_json_f = tempfile.NamedTemporaryFile('w', delete=False)
    installed_json_f.write(json_dump(output))
    installed_json_f.flush()

    args_packages = [s.strip('"\'') for s in args.packages]

    specs = []
    if args.file:
        for fpath in args.file:
            try:
                specs.extend(specs_from_url(fpath, json=context.json))
            except UnicodeError:
                raise CondaError("Error reading file, file should be a text file containing"
                                 " packages \nconda create --help for details")
        if '@EXPLICIT' in specs:
            explicit(specs, prefix, verbose=not context.quiet, index_args=index_args)
            return

    specs.extend(specs_from_args(args_packages, json=context.json))

    specs = [MatchSpec(s).conda_build_form() for s in specs]
    print("\n\nLooking for: {}\n\n".format(specs))

    strict_priority = (context.channel_priority == ChannelPriority.STRICT)
    to_link, to_unlink = api.solve(channel_json, installed_json_f.name, specs, strict_priority)

    to_link_records, to_unlink_records = [], []

    def get_channel(c):
        for x in index:
            if str(x.channel) == c:
                return x

    for c, pkg in to_unlink:
        for i_rec in installed_pkg_recs:
            if i_rec.fn == pkg:
                to_unlink_records.append(i_rec)
                break
        else:
            print("No package record found!")

    for c, pkg, jsn_s in to_link:
        sdir = get_channel(c)
        rec = to_package_record_from_subjson(sdir, pkg, jsn_s)
        to_link_records.append(rec)

    pref_setup = PrefixSetup(
        target_prefix = prefix,
        unlink_precs  = to_unlink_records,
        link_precs    = to_link_records,
        remove_specs  = [],
        update_specs  = specs
    )

    conda_transaction = UnlinkLinkTransaction(pref_setup)
    handle_txn(conda_transaction, prefix, args, True)

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
    # elif relative_mod == '.main_update':
    #     exit_code = update(args, parser)
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

    from conda.exceptions import conda_exception_handler
    return conda_exception_handler(_wrapped_main, *args, **kwargs)
