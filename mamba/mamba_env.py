# -*- coding: utf-8 -*-
# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

# conda env equivalent environment creation

from __future__ import absolute_import, print_function

from os.path import basename
import os

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

from mamba.utils import get_env_index, to_package_record_from_subjson
import mamba.mamba_api as api
from ._version import __version__

import tempfile
import threading
import sys

def mamba_install(prefix, specs, args, env, *_, **kwargs):
    # TODO: support all various ways this happens
    # Including 'nodefaults' in the channels list disables the defaults
    channel_urls = [chan for chan in env.channels if chan != 'nodefaults']

    if 'nodefaults' not in env.channels:
        channel_urls.extend(context.channels)
    _channel_priority_map = prioritize_channels(channel_urls)

    index = get_env_index(_channel_priority_map)

    channel_json = []

    for x in index:
        # add priority here
        priority = len(_channel_priority_map) - _channel_priority_map[x.url_w_subdir][1]
        subpriority = 0 if x.channel.platform == 'noarch' else 1
        if os.path.exists(x.cache_path_solv):
            cache_file = x.cache_path_solv
        else:
            cache_file = x.cache_path_json
        channel_json.append((str(x.channel), cache_file, priority, subpriority))

    specs = [MatchSpec(s) for s in specs]
    mamba_solve_specs = [s.conda_build_form() for s in specs]

    print("\n\nLooking for: {}\n\n".format(mamba_solve_specs))

    # TODO!
    installed_json_f = tempfile.NamedTemporaryFile('w', delete=False)
    installed_json_f.write("") # stupid!
    installed_json_f.flush()

    solver_options = [(api.SOLVER_FLAG_ALLOW_DOWNGRADE, 1)]

    to_link, to_unlink = api.solve(channel_json,
                                   installed_json_f.name,
                                   mamba_solve_specs,
                                   solver_options,
                                   api.SOLVER_INSTALL,
                                   False,
                                   context.quiet,
                                   context.verbosity,
                                   __version__)

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
        update_specs  = specs,
        neutered_specs = ()
    )

    conda_transaction = UnlinkLinkTransaction(pref_setup)

    pfe = conda_transaction._get_pfe()
    pfe.execute()
    conda_transaction.execute()

    try:
        installed_json_f.close()
        os.unlink(installed_json_f.name)
    except:
        pass

conda.install = mamba_install

def main():
    from conda_env.cli.main import main
    sys.argv = sys.argv[0:1] + sys.argv[2:]
    main()