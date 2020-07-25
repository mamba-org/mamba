# -*- coding: utf-8 -*-
# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

# conda env equivalent environment creation

from __future__ import absolute_import, print_function

from os.path import basename
import os, sys

from conda._vendor.boltons.setutils import IndexedSet
from conda.base.context import context
from conda.core.solve import Solver
from conda.models.channel import Channel, prioritize_channels
from conda.models.match_spec import MatchSpec
from conda.core.link import UnlinkLinkTransaction, PrefixSetup
from conda.cli.install import handle_txn
from conda_env.installers import conda
from conda.core.prefix_data import PrefixData
from conda.core.solve import diff_for_unlink_link_precs
from conda.models.prefix_graph import PrefixGraph
from conda.common.url import split_anaconda_token

from mamba.utils import get_index, to_package_record_from_subjson, init_api_context, get_installed_jsonfile, to_txn
import mamba.mamba_api as api

import tempfile
import threading
import sys

def mamba_install(prefix, specs, args, env, *_, **kwargs):
    # TODO: support all various ways this happens
    init_api_context()
    api.Context().target_prefix = prefix
    # Including 'nodefaults' in the channels list disables the defaults
    channel_urls = [chan for chan in env.channels if chan != 'nodefaults']

    if 'nodefaults' not in env.channels:
        channel_urls.extend(context.channels)
    _channel_priority_map = prioritize_channels(channel_urls)

    index = get_index(tuple(_channel_priority_map.keys()), prepend=False)

    channel_json = []

    for subdir, chan in index:
        # add priority here
        priority = len(_channel_priority_map) - _channel_priority_map[chan.url(with_credentials=True)][1]
        subpriority = 0 if chan.platform == 'noarch' else 1
        if subdir.loaded() == False and chan.platform != 'noarch':
            # ignore non-loaded subdir if channel is != noarch
            continue

        channel_json.append((chan, subdir, priority, subpriority))

    if not (context.quiet or context.json):
        print("\n\nLooking for: {}\n\n".format(specs))

    solver_options = [(api.SOLVER_FLAG_ALLOW_DOWNGRADE, 1)]

    pool = api.Pool()
    repos = []

    # if using update
    installed_pkg_recs = []
    python_constraint = None
    if 'update' in args.func:
        installed_json_f, installed_pkg_recs = get_installed_jsonfile(prefix)
        repo = api.Repo(pool, "installed", installed_json_f.name, "")
        repo.set_installed()
        repos.append(repo)

        # Also pin the Python version if it's installed
        # If python was not specified, check if it is installed.
        # If yes, add the installed python to the specs to prevent updating it.
        if 'python' not in [MatchSpec(s).name for s in specs]:
            installed_names = [i_rec.name for i_rec in installed_pkg_recs]
            if 'python' in installed_names:
                i = installed_names.index('python')
                version = installed_pkg_recs[i].version
                python_constraint = MatchSpec('python==' + version).conda_build_form()

    for channel, subdir, priority, subpriority in channel_json:
        repo = subdir.create_repo(pool)
        repo.set_priority(priority, subpriority)
        repos.append(repo)

    solver = api.Solver(pool, solver_options)
    solver.add_jobs(specs, api.SOLVER_INSTALL)

    if python_constraint:
        solver.add_pin(python_constraint)

    success = solver.solve()
    if not success:
        print(solver.problems_to_str())
        exit(1)

    package_cache = api.MultiPackageCache(context.pkgs_dirs)
    transaction = api.Transaction(solver, package_cache)
    if not (context.quiet or context.json):
        transaction.print()
    mmb_specs, to_link, to_unlink = transaction.to_conda()

    specs_to_add = [MatchSpec(m) for m in mmb_specs[0]]

    final_precs = IndexedSet()

    conda_transaction = to_txn(specs_to_add, [], prefix, to_link, to_unlink, installed_pkg_recs, index)

    pfe = conda_transaction._get_pfe()
    pfe.execute()
    conda_transaction.execute()

conda.install = mamba_install

def main():
    from conda_env.cli.main import main
    sys.argv = sys.argv[0:1] + sys.argv[2:]
    return main()
