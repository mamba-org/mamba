# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

# conda env equivalent environment creation

from __future__ import absolute_import, print_function

import sys

from conda.base.context import context
from conda.core.prefix_data import PrefixData
from conda.core.solve import get_pinned_specs
from conda.models.channel import prioritize_channels
from conda.models.match_spec import MatchSpec
from conda_env.installers import conda

import mamba.mamba_api as api
from mamba.utils import get_installed_jsonfile, init_api_context, load_channels, to_txn


def mamba_install(prefix, specs, args, env, *_, **kwargs):
    # TODO: support all various ways this happens
    init_api_context()
    api.Context().target_prefix = prefix

    match_specs = [MatchSpec(s) for s in specs]

    # Including 'nodefaults' in the channels list disables the defaults
    channel_urls = [chan for chan in env.channels if chan != "nodefaults"]

    if "nodefaults" not in env.channels:
        channel_urls.extend(context.channels)

    for spec in match_specs:
        # CONDA TODO: correct handling for subdir isn't yet done
        spec_channel = spec.get_exact_value("channel")
        if spec_channel and spec_channel not in channel_urls:
            channel_urls.append(str(spec_channel))

    ordered_channels_dict = prioritize_channels(channel_urls)

    pool = api.Pool()
    repos = []
    index = load_channels(
        pool, tuple(ordered_channels_dict.keys()), repos, prepend=False
    )

    if not (context.quiet or context.json):
        print("\n\nLooking for: {}\n\n".format(specs))

    solver_options = [(api.SOLVER_FLAG_ALLOW_DOWNGRADE, 1)]

    installed_pkg_recs = []

    # We check for installed packages even while creating a new
    # Conda environment as virtual packages such as __glibc are
    # always available regardless of the environment.
    installed_json_f, installed_pkg_recs = get_installed_jsonfile(prefix)
    repo = api.Repo(pool, "installed", installed_json_f.name, "")
    repo.set_installed()
    repos.append(repo)

    solver = api.Solver(pool, solver_options)

    # Also pin the Python version if it's installed
    # If python was not specified, check if it is installed.
    # If yes, add the installed python to the specs to prevent updating it.
    if "python" not in [s.name for s in match_specs]:
        installed_names = [i_rec.name for i_rec in installed_pkg_recs]
        if "python" in installed_names:
            i = installed_names.index("python")
            version = installed_pkg_recs[i].version
            python_constraint = MatchSpec("python==" + version).conda_build_form()
            solver.add_pin(python_constraint)

    pinned_specs = get_pinned_specs(prefix)
    if pinned_specs:
        conda_prefix_data = PrefixData(prefix)
    for s in pinned_specs:
        x = conda_prefix_data.query(s.name)
        if x:
            for el in x:
                if not s.match(el):
                    print(
                        "Your pinning does not match what's currently installed."
                        " Please remove the pin and fix your installation"
                    )
                    print("  Pin: {}".format(s))
                    print("  Currently installed: {}".format(el))
                    exit(1)
        solver.add_pin(str(s))

    solver.add_jobs(specs, api.SOLVER_INSTALL)

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

    conda_transaction = to_txn(
        specs_to_add, [], prefix, to_link, to_unlink, installed_pkg_recs, index
    )

    pfe = conda_transaction._get_pfe()
    pfe.execute()
    conda_transaction.execute()


conda.install = mamba_install


def main():
    from conda_env.cli.main import main as conda_env_main

    sys.argv = sys.argv[0:1] + sys.argv[2:]
    return conda_env_main()
