# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

from __future__ import absolute_import, division, print_function, unicode_literals

import codecs
import os
import sys
from logging import getLogger
from os.path import isdir, isfile, join
from pathlib import Path

# create support
from conda.base.constants import ChannelPriority, DepsModifier, UpdateModifier
from conda.base.context import context
from conda.cli.common import check_non_admin, confirm_yn, specs_from_url
from conda.cli.conda_argparse import generate_parser
from conda.cli.install import check_prefix, clone, get_revision
from conda.cli.main import init_loggers
from conda.common.compat import on_win
from conda.common.path import paths_equal
from conda.core.envs_manager import unregister_env
from conda.core.link import PrefixSetup, UnlinkLinkTransaction
from conda.core.prefix_data import PrefixData
from conda.core.solve import get_pinned_specs
from conda.exceptions import (
    ArgumentError,
    CondaEnvironmentError,
    CondaOSError,
    CondaValueError,
    DirectoryNotACondaEnvironmentError,
    EnvironmentLocationNotFound,
    NoBaseEnvironmentError,
    PackageNotInstalledError,
    PackagesNotFoundError,
    TooManyArgumentsError,
)
from conda.gateways.disk.create import mkdir_p
from conda.gateways.disk.delete import delete_trash, path_is_clean, rm_rf
from conda.gateways.disk.test import is_conda_environment
from conda.misc import explicit, touch_nonadmin
from conda.models.channel import MultiChannel
from conda.models.match_spec import MatchSpec

import libmambapy as api
import mamba
from mamba import repoquery as repoquery_api
from mamba.linking import handle_txn
from mamba.mamba_shell_init import shell_init
from mamba.utils import (
    get_installed_jsonfile,
    init_api_context,
    load_channels,
    print_activate,
    to_txn,
)

if sys.version_info < (3, 2):
    sys.stdout = codecs.lookup("utf-8")[-1](sys.stdout)
elif sys.version_info < (3, 7):
    sys.stdout = codecs.getwriter("utf-8")(sys.stdout.detach())
else:
    sys.stdout.reconfigure(encoding="utf-8")


log = getLogger(__name__)
stderrlog = getLogger("conda.stderr")


class MambaException(Exception):
    pass


solver_options = [(api.SOLVER_FLAG_ALLOW_DOWNGRADE, 1)]


def specs_from_args(args, json=False):
    def arg2spec(arg, json=False, update=False):
        try:
            spec = MatchSpec(arg)
        except Exception:
            raise CondaValueError("invalid package specification: %s" % arg)

        name = spec.name
        if not spec._is_simple() and update:
            raise CondaValueError(
                """version specifications not allowed with 'update'; use
        conda update  %s%s  or
        conda install %s"""
                % (name, " " * (len(arg) - len(name)), arg)
            )

        return spec

    return [arg2spec(arg, json=json) for arg in args]


use_mamba_experimental = False


def remove(args, parser):
    if not (args.all or args.package_names):
        raise CondaValueError(
            "no package names supplied,\n"
            '       try "mamba remove -h" for more details'
        )

    prefix = context.target_prefix
    check_non_admin()
    init_api_context()

    if args.all and prefix == context.default_prefix:
        raise CondaEnvironmentError(
            "cannot remove current environment. \
                                     deactivate and run mamba remove again"
        )

    if args.all and path_is_clean(prefix):
        # full environment removal was requested, but environment doesn't exist anyway
        return 0

    if args.all:
        if prefix == context.root_prefix:
            raise CondaEnvironmentError(
                "cannot remove root environment,\n"
                "       add -n NAME or -p PREFIX option"
            )
        print("\nRemove all packages in environment %s:\n" % prefix, file=sys.stderr)

        if "package_names" in args:
            stp = PrefixSetup(
                target_prefix=prefix,
                unlink_precs=tuple(PrefixData(prefix).iter_records()),
                link_precs=(),
                remove_specs=(),
                update_specs=(),
                neutered_specs=(),
            )
            txn = UnlinkLinkTransaction(stp)
            try:
                handle_txn(txn, prefix, args, False, True)
            except PackagesNotFoundError:
                print(
                    "No packages found in %s. Continuing environment removal" % prefix
                )

        rm_rf(prefix, clean_empty_parents=True)
        unregister_env(prefix)

        return

    else:
        if args.features:
            specs = tuple(MatchSpec(track_features=f) for f in set(args.package_names))
        else:
            specs = [s for s in specs_from_args(args.package_names)]
        if not context.quiet:
            print("Removing specs: {}".format([s.conda_build_form() for s in specs]))

        installed_json_f, installed_pkg_recs = get_installed_jsonfile(prefix)

        mamba_solve_specs = [s.conda_build_form() for s in specs]

        solver_options.append((api.SOLVER_FLAG_ALLOW_UNINSTALL, 1))

        if context.channel_priority is ChannelPriority.STRICT:
            solver_options.append((api.SOLVER_FLAG_STRICT_REPO_PRIORITY, 1))

        pool = api.Pool()
        repos = []

        # add installed
        prefix_data = api.PrefixData(context.target_prefix)
        repo = api.Repo(pool, prefix_data)
        repos.append(repo)

        solver = api.Solver(pool, solver_options)

        if args.force_remove:
            solver.set_postsolve_flags([(api.MAMBA_NO_DEPS, True)])

        history = api.History(context.target_prefix)
        history_map = history.get_requested_specs_map()
        solver.add_jobs(
            [ms.conda_build_form() for ms in history_map.values()],
            api.SOLVER_USERINSTALLED,
        )

        solver.add_jobs(mamba_solve_specs, api.SOLVER_ERASE | api.SOLVER_CLEANDEPS)
        success = solver.try_solve()
        if not success:
            print(solver.explain_problems())
            exit_code = 1
            return exit_code

        package_cache = api.MultiPackageCache(context.pkgs_dirs)
        transaction = api.Transaction(pool, solver, package_cache)

        if not transaction.prompt():
            exit(0)
        elif not context.dry_run:
            transaction.fetch_extract_packages()

        mmb_specs, to_link, to_unlink = transaction.to_conda()
        transaction.log_json()

        specs_to_add = [MatchSpec(m) for m in mmb_specs[0]]
        specs_to_remove = [MatchSpec(m) for m in mmb_specs[1]]

        conda_transaction = to_txn(
            specs_to_add,
            specs_to_remove,
            prefix,
            to_link,
            to_unlink,
            installed_pkg_recs,
        )
        handle_txn(conda_transaction, prefix, args, False, True)


def install(args, parser, command="install"):
    """
    mamba install, mamba update, and mamba create
    """
    context.validate_configuration()
    check_non_admin()

    api.init_console()

    init_api_context(use_mamba_experimental)

    newenv = bool(command == "create")
    isinstall = bool(command == "install")
    solver_task = api.SOLVER_INSTALL

    isupdate = bool(command == "update")
    if isupdate:
        solver_task = api.SOLVER_UPDATE
        solver_options.clear()

    if newenv and not (args.name or args.prefix):
        raise CondaValueError(
            "either -n NAME or -p PREFIX option required,\n"
            'try "mamba %s -h" for more details' % command
        )
    prefix = context.target_prefix
    if newenv:
        check_prefix(prefix, json=context.json)
    if context.force_32bit and prefix == context.root_prefix:
        raise CondaValueError("cannot use CONDA_FORCE_32BIT=1 in base env")
    if isupdate and not (
        args.file
        or args.packages
        or context.update_modifier == UpdateModifier.UPDATE_ALL
    ):
        raise CondaValueError(
            """no package names supplied
# If you want to update to a newer version of Anaconda, type:
#
# $ conda update --prefix %s anaconda
"""
            % prefix
        )

    if not newenv:
        if isdir(prefix):
            if on_win:
                delete_trash(prefix)

            if not isfile(join(prefix, "conda-meta", "history")):
                if paths_equal(prefix, context.conda_prefix):
                    raise NoBaseEnvironmentError()
                else:
                    if not path_is_clean(prefix):
                        raise DirectoryNotACondaEnvironmentError(prefix)
            else:
                # fall-through expected under normal operation
                pass
        else:
            if hasattr(args, "mkdir") and args.mkdir:
                try:
                    mkdir_p(prefix)
                except EnvironmentError as e:
                    raise CondaOSError(
                        "Could not create directory: %s" % prefix, caused_by=e
                    )
            else:
                raise EnvironmentLocationNotFound(prefix)

    prefix = context.target_prefix

    #############################
    # Get SPECS                 #
    #############################

    args_packages = [s.strip("\"'") for s in args.packages]
    if newenv and not args.no_default_packages:
        # Override defaults if they are specified at the command line
        # TODO: rework in 4.4 branch using MatchSpec
        args_packages_names = [
            pkg.replace(" ", "=").split("=", 1)[0] for pkg in args_packages
        ]
        for default_pkg in context.create_default_packages:
            default_pkg_name = default_pkg.replace(" ", "=").split("=", 1)[0]
            if default_pkg_name not in args_packages_names:
                args_packages.append(default_pkg)

    num_cp = sum(
        (s.endswith(".tar.bz2") or s.endswith(".conda")) for s in args_packages
    )
    if num_cp:
        if num_cp == len(args_packages):
            explicit(args_packages, prefix, verbose=not (context.quiet or context.json))
            return
        else:
            raise CondaValueError(
                "cannot mix specifications with conda package" " filenames"
            )

    specs = []

    index_args = {
        "use_cache": args.use_index_cache,
        "channel_urls": context.channels,
        "unknown": args.unknown,
        "prepend": not args.override_channels,
        "use_local": args.use_local,
    }

    if args.file:
        file_specs = []
        for fpath in args.file:
            try:
                file_specs += specs_from_url(fpath, json=context.json)
            except UnicodeError:
                raise CondaValueError(
                    "Error reading file, file should be a text file containing"
                    " packages \nconda create --help for details"
                )
        if "@EXPLICIT" in file_specs:
            explicit(
                file_specs,
                prefix,
                verbose=not (context.quiet or context.json),
                index_args=index_args,
            )
            return

        specs.extend(MatchSpec(s) for s in file_specs)

    specs.extend(specs_from_args(args_packages, json=context.json))

    # update channels from package specs (e.g. mychannel::mypackage adds mychannel)
    channels = [c for c in context.channels]
    for spec in specs:
        # CONDA TODO: correct handling for subdir isn't yet done
        spec_channel = spec.get_exact_value("channel")
        if spec_channel:
            if isinstance(spec_channel, MultiChannel):
                channels.append(spec_channel.name)
            elif spec_channel.base_url not in channels:
                channels.append(spec_channel.base_url)

    index_args["channel_urls"] = channels

    installed_json_f, installed_pkg_recs = get_installed_jsonfile(prefix)

    if isinstall and args.revision:
        get_revision(args.revision, json=context.json)
    elif isinstall and not (args.file or args_packages):
        raise CondaValueError(
            "too few arguments, " "must supply command line package specs or --file"
        )

    installed_names = [i_rec.name for i_rec in installed_pkg_recs]
    # for 'conda update', make sure the requested specs actually exist in the prefix
    # and that they are name-only specs
    if isupdate and context.update_modifier == UpdateModifier.UPDATE_ALL:
        for i in installed_names:
            if i.startswith("__"):
                continue
            specs.append(MatchSpec(i))

        prefix_data = PrefixData(prefix)
        for s in args_packages:
            s = MatchSpec(s)
            if s.name == "python":
                specs.append(s)
            if not s.is_name_only_spec:
                raise CondaValueError(
                    "Invalid spec for 'conda update': %s\n"
                    "Use 'conda install' instead." % s
                )
            if not prefix_data.get(s.name, None):
                raise PackageNotInstalledError(prefix, s.name)

    elif context.update_modifier == UpdateModifier.UPDATE_DEPS:
        # find the deps for each package and add to the update job
        # solver_task |= api.SOLVER_FORCEBEST
        final_specs = specs
        for spec in specs:
            prec = installed_pkg_recs[installed_names.index(spec.name)]
            for dep in prec.depends:
                ms = MatchSpec(dep)
                if ms.name != "python":
                    final_specs.append(MatchSpec(ms.name))
        specs = list(set(final_specs))

    if newenv and args.clone:
        if args.packages:
            raise TooManyArgumentsError(
                0,
                len(args.packages),
                list(args.packages),
                "did not expect any arguments for --clone",
            )

        clone(
            args.clone,
            prefix,
            json=context.json,
            quiet=(context.quiet or context.json),
            index_args=index_args,
        )
        touch_nonadmin(prefix)
        print_activate(args.name if args.name else prefix)
        return

    if not (context.quiet or context.json):
        print("\nLooking for: {}\n".format([str(s) for s in specs]))

    # If python was not specified, check if it is installed.
    # If yes, add the installed python to the specs to prevent updating it.
    python_constraint = None

    if "python" in installed_names:
        if context.update_modifier == UpdateModifier.UPDATE_ALL or not any(
            s.name == "python" for s in specs
        ):
            version = installed_pkg_recs[installed_names.index("python")].version
            major_minor_version = ".".join(version.split(".")[:2])
            python_constraint = f"python {major_minor_version}.*"

    mamba_solve_specs = [s.__str__() for s in specs]

    if context.channel_priority is ChannelPriority.STRICT:
        solver_options.append((api.SOLVER_FLAG_STRICT_REPO_PRIORITY, 1))

    pool = api.Pool()

    repos = []

    # add installed
    prefix_data = api.PrefixData(context.target_prefix)
    prefix_data.add_packages(api.get_virtual_packages())
    repo = api.Repo(pool, prefix_data)
    repos.append(repo)

    if newenv and not specs:
        # creating an empty environment with e.g. "mamba create -n my_env"
        # should not download the repodata
        index = []
        specs_to_add = []
        specs_to_remove = []
        to_link = []
        to_unlink = []
        installed_pkg_recs = []
        transaction = None
    else:
        index = load_channels(pool, channels, repos)

        solver = api.Solver(pool, solver_options)

        solver.set_postsolve_flags(
            [
                (api.MAMBA_NO_DEPS, context.deps_modifier == DepsModifier.NO_DEPS),
                (api.MAMBA_ONLY_DEPS, context.deps_modifier == DepsModifier.ONLY_DEPS),
                (api.MAMBA_FORCE_REINSTALL, context.force_reinstall),
            ]
        )

        if context.update_modifier is UpdateModifier.FREEZE_INSTALLED:
            solver.add_jobs([p for p in prefix_data.package_records], api.SOLVER_LOCK)

        solver.add_jobs(mamba_solve_specs, solver_task)

        if not context.force_reinstall:
            # as a security feature this will _always_ attempt to upgradecertain
            # packages
            for a_pkg in [_.name for _ in context.aggressive_update_packages]:
                if a_pkg in installed_names:
                    solver.add_jobs([a_pkg], api.SOLVER_UPDATE)

        pinned_specs_info = ""
        if python_constraint:
            pinned_specs_info += f"  - {python_constraint}\n"
            solver.add_pin(python_constraint)

        pinned_specs = get_pinned_specs(context.target_prefix)
        if pinned_specs:
            conda_prefix_data = PrefixData(context.target_prefix)
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
                    f"\nERROR: could not add pinned spec {s}. Make sure pin"
                    "is of the format\n"
                    "libname VERSION BUILD, for example libblas=*=*mkl\n"
                )

        if pinned_specs_info and not (context.quiet or context.json):
            print(f"\nPinned packages:\n{pinned_specs_info}\n")

        success = solver.try_solve()
        if not success:
            print(solver.explain_problems())
            exit_code = 1
            return exit_code

        package_cache = api.MultiPackageCache(context.pkgs_dirs)
        transaction = api.Transaction(pool, solver, package_cache)
        mmb_specs, to_link, to_unlink = transaction.to_conda()

        specs_to_add = [MatchSpec(m) for m in mmb_specs[0]]
        specs_to_remove = [MatchSpec(m) for m in mmb_specs[1]]

        transaction.log_json()

    if use_mamba_experimental and transaction is not None:
        if transaction.prompt():
            if (
                newenv
                and not isdir(Path(prefix) / "conda-meta")
                and not context.dry_run
            ):
                mkdir_p(Path(prefix) / "conda-meta")

            transaction.execute(prefix_data)
    else:
        if transaction is not None:
            if transaction.prompt() is False:
                exit(0)
            if not context.dry_run:
                transaction.fetch_extract_packages()

        conda_transaction = to_txn(
            specs_to_add,
            specs_to_remove,
            prefix,
            to_link,
            to_unlink,
            installed_pkg_recs,
            index,
        )
        handle_txn(conda_transaction, prefix, args, newenv)

    if newenv:
        touch_nonadmin(prefix)
        print_activate(args.name if args.name else prefix)

    try:
        installed_json_f.close()
        os.unlink(installed_json_f.name)
    except Exception:
        pass


def create(args, parser):
    if is_conda_environment(context.target_prefix):
        if paths_equal(context.target_prefix, context.root_prefix):
            raise CondaValueError("The target prefix is the base prefix. Aborting.")
        confirm_yn(
            "WARNING: A conda environment already exists at '%s'\n"
            "Remove existing environment" % context.target_prefix,
            default="no",
            dry_run=False,
        )
        log.info("Removing existing environment %s", context.target_prefix)
        rm_rf(context.target_prefix)
    elif isdir(context.target_prefix):
        confirm_yn(
            "WARNING: A directory already exists at the target location '%s'\n"
            "but it is not a conda environment.\n"
            "Continue creating environment" % context.target_prefix,
            default="no",
            dry_run=False,
        )
    return install(args, parser, "create")


def update(args, parser):
    if context.force:
        print(
            "\n\n"
            "WARNING: The --force flag will be removed in a future conda release.\n"
            "         See 'conda update --help'"
            " for details about the --force-reinstall\n"
            "         and --clobber flags.\n"
            "\n",
            file=sys.stderr,
        )

    # need to implement some modifications on the update function
    return install(args, parser, "update")


def repoquery(args, parser):
    if not args.subcmd:
        print("repoquery needs a subcommand (search, depends or whoneeds)")
        print("eg:")
        print("    $ mamba repoquery search xtensor\n")
        exit(1)

    # print(args)
    if args.platform:
        platform = args.platform
    else:
        platform = context.subdir

    channels = None
    if hasattr(args, "channel"):
        channels = args.channel
    if args.all_channels or (channels is None and args.subcmd == "search"):
        if channels:
            print("WARNING: Using all channels instead of configured channels")
        channels = context.channels

    use_installed = args.installed
    if args.no_installed:
        use_installed = False

    # if we're asking for depends and channels are given, disregard
    # installed packages to prevent weird mixing
    if args.subcmd in ("depends", "whoneeds") and use_installed and channels:
        use_installed = False

    only_installed = True
    if args.subcmd == "search" and not args.installed:
        only_installed = False
    elif args.all_channels or (channels and len(channels)):
        only_installed = False

    if only_installed and args.no_installed:
        print("No channels selected.")
        print("Activate -a to search all channels.")
        exit(1)

    if not context.json:
        print("\nExecuting the query %s\n" % args.package_query)

    if context.json:
        fmt = api.QueryFormat.JSON
    elif hasattr(args, "tree") and args.tree:
        fmt = api.QueryFormat.TREE
    elif hasattr(args, "recursive") and args.recursive:
        fmt = api.QueryFormat.RECURSIVETABLE
    elif hasattr(args, "pretty") and args.pretty:
        fmt = api.QueryFormat.PRETTY
    else:
        fmt = api.QueryFormat.TABLE

    pool = repoquery_api.create_pool(channels, platform, use_installed)

    print("\n")
    print(repoquery_api._repoquery(args.subcmd, args.package_query, pool, fmt))


def clean(args, parser):
    if args.locks:
        init_api_context()

        root_prefix = os.environ.get("MAMBA_ROOT_PREFIX")
        if api.Context().prefix_params.root_prefix != root_prefix:
            os.environ["MAMBA_ROOT_PREFIX"] = str(
                api.Context().prefix_params.root_prefix
            )

        api.clean(api.MAMBA_CLEAN_LOCKS)
        if root_prefix:
            os.environ["MAMBA_ROOT_PREFIX"] = root_prefix

    try:
        from importlib import import_module

        relative_mod, func_name = args.func.rsplit(".", 1)
        # make module relative following https://github.com/conda/conda/pull/13173
        if relative_mod.startswith("conda.cli."):
            relative_mod = f'.{relative_mod.split(".")[-1]}'

        module = import_module("conda.cli" + relative_mod, __name__.rsplit(".", 1)[0])
        exit_code = getattr(module, func_name)(args, parser)
        return exit_code
    except ArgumentError as e:
        if not args.locks:
            raise e


def do_call(args, parser):
    if hasattr(args, "func"):
        relative_mod, func_name = args.func.rsplit(".", 1)
        # make module relative following https://github.com/conda/conda/pull/13173
        if relative_mod.startswith("conda.cli."):
            relative_mod = f'.{relative_mod.split(".")[-1]}'
    elif hasattr(args, "_plugin_subcommand"):
        action = args._plugin_subcommand.action
        relative_mod = f'.{action.__module__.split(".")[-1]}'
        func_name = action.__name__
    else:
        raise ValueError("Unrecognized 'args' object: %r" % args)

    # func_name should always be 'execute'
    if relative_mod in [
        ".main_list",
        ".main_search",
        ".main_run",
        ".main_info",
    ]:
        from importlib import import_module

        module = import_module("conda.cli" + relative_mod, __name__.rsplit(".", 1)[0])
        if relative_mod == ".main_info":
            import io
            from contextlib import redirect_stdout

            with io.StringIO() as buf, redirect_stdout(buf):
                exit_code = getattr(module, func_name)(args, parser)
                output = buf.getvalue()
            # Append mamba version to output before printing/serializing
            if context.json:
                from json import loads

                from conda.common.serialize import json_dump

                json_obj = loads(output)
                json_obj["mamba_version"] = mamba.__version__
                print(json_dump(json_obj))  # To match conda's format
            else:

                def format_param(nm, val):
                    return f"{nm:>23} : {val}"  # To match conda's format

                print("\n" + format_param("mamba version", mamba.__version__))
                print(output.lstrip("\n"))
        else:
            exit_code = getattr(module, func_name)(args, parser)
    elif relative_mod == ".main_clean":
        exit_code = clean(args, parser)
    elif relative_mod == ".main_install":
        exit_code = install(args, parser, "install")
    elif relative_mod == ".main_remove":
        exit_code = remove(args, parser)
    elif relative_mod == ".main_create":
        exit_code = create(args, parser)
    elif relative_mod == ".main_update":
        exit_code = update(args, parser)
    elif relative_mod == ".main_init":
        exit_code = shell_init(args)
    elif relative_mod in (".main_repoquery", ".repoquery"):
        exit_code = repoquery(args, parser)
    else:
        print(
            "Currently, only install, create, list, search, run,"
            " info, clean, remove, update, repoquery, activate and"
            " deactivate are supported through mamba."
        )

        return 1
    return exit_code


def configure_clean_locks(sub_parsers):
    removal_target_options = {
        g.title: g for g in sub_parsers.choices["clean"]._action_groups
    }["Removal Targets"]
    removal_target_options.add_argument(
        "--locks",
        action="store_true",
        help="Remove lock files.",
    )


def configure_parser_repoquery(sub_parsers):
    help_cli = "Query repositories using mamba. "
    descr = help_cli

    example = """
Examples:

    mamba repoquery search xtensor>=0.18
    mamba repoquery depends xtensor
    mamba repoquery whoneeds xtl

    """

    import argparse
    from argparse import SUPPRESS

    try:
        p = sub_parsers.add_parser(
            "repoquery", description=descr, help=help_cli, epilog=example
        )
    except argparse.ArgumentError as exc:
        if "conflicting subparser" in str(exc):
            # conda-libmamba-solver's repoquery is already registered
            return
        raise

    subsub_parser = p.add_subparsers(dest="subcmd")
    package_cmds = argparse.ArgumentParser(add_help=False)
    package_cmds.add_argument("package_query", help="the target package")
    package_cmds.add_argument(
        "-i", "--installed", action="store_true", default=True, help=SUPPRESS
    )

    package_cmds.add_argument("-p", "--platform")
    package_cmds.add_argument("--no-installed", action="store_true")
    package_cmds.add_argument("--pretty", action="store_true")

    package_cmds.add_argument(
        "-a",
        "--all-channels",
        action="store_true",
        help="Look at all channels (for depends / whoneeds)",
    )

    view_cmds = argparse.ArgumentParser(add_help=False)
    view_cmds.add_argument("-t", "--tree", action="store_true")
    view_cmds.add_argument("--recursive", action="store_true")

    c1 = subsub_parser.add_parser(
        "whoneeds",
        help="shows packages that depend on this package",
        parents=[package_cmds, view_cmds],
    )

    c2 = subsub_parser.add_parser(
        "depends",
        help="shows dependencies of this package",
        parents=[package_cmds, view_cmds],
    )

    c3 = subsub_parser.add_parser(
        "search",
        help="shows all available package versions",
        parents=[package_cmds],
    )

    from conda.cli import conda_argparse

    for cmd in (c1, c2, c3):
        conda_argparse.add_parser_channels(cmd)
        conda_argparse.add_parser_networking(cmd)
        conda_argparse.add_parser_known(cmd)
        conda_argparse.add_parser_json(cmd)

    p.set_defaults(func=".main_repoquery.execute")
    return p


def _wrapped_main(*args, **kwargs):
    if len(args) == 1:
        args = args + ("-h",)

    args = list(args)

    if "--mamba-experimental" in args:
        global use_mamba_experimental
        use_mamba_experimental = True
        args.remove("--mamba-experimental")

    if "--no-banner" in args:
        # Backwards compat (banner was removed)
        args.remove("--no-banner")

    p = generate_parser()
    configure_clean_locks(p._subparsers._group_actions[0])
    configure_parser_repoquery(p._subparsers._group_actions[0])
    parsed_args = p.parse_args(args[1:])

    context.__init__(argparse_args=parsed_args)
    context.__initialized__ = True

    init_loggers()
    result = do_call(parsed_args, p)
    exit_code = getattr(
        result, "rc", result
    )  # may be Result objects with code in rc field
    if isinstance(exit_code, int):
        return exit_code


# Main entry point!
def main(*args, **kwargs):
    # Set to false so we don't allow uploading our issues to conda!
    context.report_errors = False

    from conda.common.compat import ensure_text_type

    if "activate" in sys.argv or "deactivate" in sys.argv:
        print(
            "Run 'mamba init' to be able to run mamba activate/deactivate\n"
            "and start a new shell session. Or use conda to activate/deactivate.\n",
            file=sys.stderr,
        )
        return sys.exit(-1)

    if not args:
        args = sys.argv

    if len(args) == 2 and args[1] in ("--version", "-V"):
        from mamba._version import __version__

        print("mamba {}".format(__version__))

    args = tuple(ensure_text_type(s) for s in args)

    if len(args) >= 2:
        if args[1] == "env":
            # special handling for conda env create!
            from mamba import mamba_env

            return mamba_env.main()
        elif args[1] == "build":
            # calling mamba build == conda mambabuild
            try:
                from boa.cli import mambabuild

                return mambabuild.main()
            except ImportError as exc:
                raise ImportError(
                    "Please install boa to use mamba build:\n"
                    "  $ mamba install -c conda-forge boa"
                ) from exc

    def exception_converter(*args, **kwargs):
        exit_code = 0
        try:
            exit_code = _wrapped_main(*args, **kwargs)
        except api.MambaNativeException as e:
            print(e, file=sys.stderr)
        except MambaException as e:
            print(e, file=sys.stderr)
        except Exception as e:
            print(e, file=sys.stderr)
            raise e
        return exit_code

    from conda.exceptions import conda_exception_handler

    return conda_exception_handler(exception_converter, *args, **kwargs)


if __name__ == "__main__":
    main()
