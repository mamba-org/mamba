# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

# conda env equivalent environment creation

from __future__ import absolute_import, print_function

import os
import sys
import tempfile

from conda.base.constants import ChannelPriority
from conda.base.context import context
from conda.core.prefix_data import PrefixData
from conda.core.solve import get_pinned_specs
from conda.models.match_spec import MatchSpec
from conda_env.installers import conda

import libmambapy as api
from mamba.linking import handle_txn
from mamba.utils import (
    compute_final_precs,
    get_installed_jsonfile,
    init_api_context,
    load_channels,
    to_txn,
    to_txn_precs,
)


def mamba_install(prefix, specs, args, env, dry_run=False, *_, **kwargs):
    # TODO: support all various ways this happens
    init_api_context()
    api.Context().prefix_params.target_prefix = prefix
    # conda doesn't ask for confirmation with env
    api.Context().always_yes = True

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

    pool = api.Pool()
    repos = []
    index = load_channels(pool, channel_urls, repos, prepend=False)

    if not (context.quiet or context.json):
        print("\n\nLooking for: {}\n\n".format(specs))

    solver_options = [(api.SOLVER_FLAG_ALLOW_DOWNGRADE, 1)]

    if context.channel_priority is ChannelPriority.STRICT:
        solver_options.append((api.SOLVER_FLAG_STRICT_REPO_PRIORITY, 1))

    installed_pkg_recs = []

    prune = getattr(args, "prune", False)

    # We check for installed packages even while creating a new
    # Conda environment as virtual packages such as __glibc are
    # always available regardless of the environment.
    installed_json_f, installed_pkg_recs = get_installed_jsonfile(prefix)
    if prune:
        try:
            installed_json_f.close()
            os.unlink(installed_json_f.name)
        except Exception:
            pass
        installed_pkg_recs_prefix = installed_pkg_recs
        with tempfile.TemporaryDirectory() as td:
            installed_json_f, installed_pkg_recs = get_installed_jsonfile(td)

    prefix_data = api.PrefixData(prefix)
    prefix_data.add_packages(api.get_virtual_packages())
    repo = api.Repo(pool, prefix_data)
    repo.set_installed()
    repos.append(repo)

    solver = api.Solver(pool, solver_options)

    # Also pin the Python version if it's installed
    # If python was not specified, check if it is installed.
    # If yes, add the installed python to the specs to prevent updating it.
    installed_names = [i_rec.name for i_rec in installed_pkg_recs]
    if "python" not in [s.name for s in match_specs]:
        if "python" in installed_names:
            i = installed_names.index("python")
            version = installed_pkg_recs[i].version
            python_constraint = MatchSpec("python==" + version).conda_build_form()
            solver.add_pin(python_constraint)

    pinned_specs = get_pinned_specs(prefix)
    pinned_specs_info = ""
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

        try:
            final_spec = s.conda_build_form()
            pinned_specs_info += f"  - {final_spec}\n"
            solver.add_pin(final_spec)
        except AssertionError:
            print(
                f"\nERROR: could not add pinned spec {s}. Make sure pin "
                "is of the format\n"
                "libname VERSION BUILD, for example libblas=*=*mkl\n"
            )

    if pinned_specs_info:
        print(f"\n  Pinned packages:\n\n{pinned_specs_info}\n")

    install_specs = [s for s in specs if MatchSpec(s).name not in installed_names]
    if install_specs:
        solver.add_jobs(install_specs, api.SOLVER_INSTALL)

    update_specs = [s for s in specs if MatchSpec(s).name in installed_names]
    if update_specs:
        solver.add_jobs(update_specs, api.SOLVER_UPDATE)

    success = solver.try_solve()
    if not success:
        print(solver.explain_problems())
        exit(1)

    package_cache = api.MultiPackageCache(context.pkgs_dirs)
    transaction = api.Transaction(pool, solver, package_cache)
    mmb_specs, to_link, to_unlink = transaction.to_conda()

    specs_to_add = [MatchSpec(m) for m in mmb_specs[0]]

    transaction.log_json()
    if not transaction.prompt():
        exit(0)
    elif not context.dry_run:
        transaction.fetch_extract_packages()

    if prune:
        history = api.History(prefix)
        history_map = history.get_requested_specs_map()
        specs_to_add_names = {m.name for m in specs_to_add}
        specs_to_remove = [
            MatchSpec(m) for m in history_map if m not in specs_to_add_names
        ]
        final_precs = compute_final_precs(
            None, to_link, to_unlink, installed_pkg_recs_prefix, index
        )
        conda_transaction = to_txn_precs(
            specs_to_add, specs_to_remove, prefix, final_precs
        )
    else:
        conda_transaction = to_txn(
            specs_to_add, [], prefix, to_link, to_unlink, installed_pkg_recs, index
        )

    handle_txn(conda_transaction, prefix, args, True)

    try:
        installed_json_f.close()
        os.unlink(installed_json_f.name)
    except Exception:
        pass


def mamba_dry_run(specs, args, env, *_, **kwargs):
    return mamba_install(
        tempfile.mkdtemp(), specs, args, env, *_, dry_run=True, **kwargs
    )


conda.install = mamba_install
conda.dry_run = mamba_dry_run


def main():
    from conda_env.cli.main import main as conda_env_main

    sys.argv = sys.argv[0:1] + sys.argv[2:]
    return conda_env_main()
