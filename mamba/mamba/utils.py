# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

from __future__ import absolute_import, division, print_function, unicode_literals

import json
import os
import sys
import tempfile
import urllib.parse
from collections import OrderedDict

from boltons.setutils import IndexedSet
from conda.base.constants import ChannelPriority
from conda.base.context import context
from conda.common.serialize import json_dump
from conda.common.url import join_url, remove_auth, split_anaconda_token
from conda.core.index import _supplement_index_with_system, check_allowlist
from conda.core.link import PrefixSetup, UnlinkLinkTransaction
from conda.core.prefix_data import PrefixData
from conda.core.solve import diff_for_unlink_link_precs
from conda.gateways.connection.session import CondaHttpAuth
from conda.models.channel import Channel as CondaChannel
from conda.models.prefix_graph import PrefixGraph
from conda.models.records import PackageRecord

import libmambapy as api


def load_channel(subdir_data, result_container):
    if not context.quiet:
        print("Getting ", subdir_data.channel.name, subdir_data.channel.platform)
    return result_container.append(subdir_data.loaded())


def get_index(
    channel_urls=(),
    prepend=True,
    platform=None,
    use_local=False,
    use_cache=False,
    unknown=None,
    prefix=None,
    repodata_fn="repodata.json",
):
    if isinstance(platform, str):
        platform = [platform, "noarch"]

    all_channels = []
    if use_local:
        all_channels.append("local")
    all_channels.extend(channel_urls)
    if prepend:
        all_channels.extend(context.channels)
    check_allowlist(all_channels)

    # Remove duplicates but retain order
    all_channels = list(OrderedDict.fromkeys(all_channels))

    dlist = api.DownloadTargetList()

    index = []

    def fixup_channel_spec(spec):
        at_count = spec.count("@")
        if at_count > 1:
            first_at = spec.find("@")
            spec = (
                spec[:first_at]
                + urllib.parse.quote(spec[first_at])
                + spec[first_at + 1 :]
            )
        if platform:
            spec = spec + "[" + ",".join(platform) + "]"
        return spec

    all_channels = list(map(fixup_channel_spec, all_channels))
    pkgs_dirs = api.MultiPackageCache(context.pkgs_dirs)
    api.create_cache_dir(str(pkgs_dirs.first_writable_path))

    for channel in api.get_channels(all_channels):
        for channel_platform, url in channel.platform_urls(with_credentials=True):
            full_url = CondaHttpAuth.add_binstar_token(url)

            sd = api.SubdirData(
                channel, channel_platform, full_url, pkgs_dirs, repodata_fn
            )

            needs_finalising = sd.download_and_check_targets(dlist)
            index.append(
                (
                    sd,
                    {
                        "platform": channel_platform,
                        "url": url,
                        "channel": channel,
                        "needs_finalising": needs_finalising,
                    },
                )
            )

    for sd, info in index:
        if info["needs_finalising"]:
            sd.finalize_checks()
        dlist.add(sd)

    is_downloaded = dlist.download(api.MAMBA_DOWNLOAD_FAILFAST)

    if not is_downloaded:
        raise RuntimeError("Error downloading repodata.")

    return index


def load_channels(
    pool,
    channels,
    repos,
    has_priority=None,
    prepend=True,
    platform=None,
    use_local=False,
    use_cache=True,
    repodata_fn="repodata.json",
):
    index = get_index(
        channel_urls=channels,
        prepend=prepend,
        platform=platform,
        use_local=use_local,
        repodata_fn=repodata_fn,
        use_cache=use_cache,
    )

    if has_priority is None:
        has_priority = context.channel_priority in [
            ChannelPriority.STRICT,
            ChannelPriority.FLEXIBLE,
        ]

    subprio_index = len(index)
    if has_priority:
        # first, count unique channels
        n_channels = len(set([entry["channel"].canonical_name for _, entry in index]))
        current_channel = index[0][1]["channel"].canonical_name
        channel_prio = n_channels

    for subdir, entry in index:
        # add priority here
        if has_priority:
            if entry["channel"].canonical_name != current_channel:
                channel_prio -= 1
                current_channel = entry["channel"].canonical_name
            priority = channel_prio
        else:
            priority = 0
        if has_priority:
            subpriority = 0
        else:
            subpriority = subprio_index
            subprio_index -= 1

        if not subdir.loaded() and entry["platform"] != "noarch":
            # ignore non-loaded subdir if channel is != noarch
            continue

        if context.verbosity != 0 and not context.json:
            print(
                "Channel: {}, platform: {}, prio: {} : {}".format(
                    entry["channel"], entry["platform"], priority, subpriority
                )
            )
            print("Cache path: ", subdir.cache_path())

        repo = subdir.create_repo(pool)
        repo.set_priority(priority, subpriority)
        repos.append(repo)

    return index


def log_level_from_verbosity(verbosity: int):
    if verbosity == 0:
        return api.LogLevel.WARNING
    elif verbosity == 1:
        return api.LogLevel.INFO
    elif verbosity == 2:
        return api.LogLevel.DEBUG
    else:
        return api.LogLevel.TRACE


def init_api_context(use_mamba_experimental=False):
    api_ctx = api.Context()

    api_ctx.output_params.json = context.json
    api_ctx.dry_run = context.dry_run
    if context.json:
        context.always_yes = True
        context.quiet = True
        if use_mamba_experimental:
            context.json = False

    api_ctx.output_params.verbosity = context.verbosity
    api_ctx.set_verbosity(context.verbosity)
    api_ctx.output_params.quiet = context.quiet
    api_ctx.offline = context.offline
    api_ctx.local_repodata_ttl = context.local_repodata_ttl
    api_ctx.use_index_cache = context.use_index_cache
    api_ctx.always_yes = context.always_yes
    api_ctx.channels = context.channels
    api_ctx.platform = context.subdir
    # Conda uses a frozendict here
    api_ctx.remote_fetch_params.proxy_servers = dict(context.proxy_servers)

    if "MAMBA_EXTRACT_THREADS" in os.environ:
        try:
            max_threads = int(os.environ["MAMBA_EXTRACT_THREADS"])
            api_ctx.threads_params.extract_threads = max_threads
        except ValueError:
            v = os.environ["MAMBA_EXTRACT_THREADS"]
            raise ValueError(
                f"Invalid conversion of env variable 'MAMBA_EXTRACT_THREADS' from value '{v}'"
            )

    def get_base_url(url, name=None):
        tmp = url.rsplit("/", 1)[0]
        if name:
            if tmp.endswith(name):
                # Return base url stripped from name
                return tmp[: -(len(name) + 1)]
        return tmp

    api_ctx.channel_alias = str(
        get_base_url(context.channel_alias.url(with_credentials=True))
    )

    additional_custom_channels = {}
    for el in context.custom_channels:
        if context.custom_channels[el].canonical_name not in ["local", "defaults"]:
            additional_custom_channels[el] = get_base_url(
                context.custom_channels[el].url(with_credentials=True), el
            )
    api_ctx.custom_channels = additional_custom_channels

    additional_custom_multichannels = {}
    for el in context.custom_multichannels:
        if el not in ["defaults", "local"]:
            additional_custom_multichannels[el] = []
            for c in context.custom_multichannels[el]:
                additional_custom_multichannels[el].append(
                    get_base_url(c.url(with_credentials=True))
                )
    api_ctx.custom_multichannels = additional_custom_multichannels

    api_ctx.default_channels = [
        get_base_url(x.url(with_credentials=True)) for x in context.default_channels
    ]

    if context.ssl_verify is False:
        api_ctx.remote_fetch_params.ssl_verify = "<false>"
    elif context.ssl_verify is not True:
        api_ctx.remote_fetch_params.ssl_verify = context.ssl_verify
    api_ctx.prefix_params.target_prefix = context.target_prefix
    api_ctx.prefix_params.root_prefix = context.root_prefix
    api_ctx.prefix_params.conda_prefix = context.conda_prefix
    api_ctx.pkgs_dirs = context.pkgs_dirs
    api_ctx.envs_dirs = context.envs_dirs

    api_ctx.remote_fetch_params.connect_timeout_secs = (
        context.remote_connect_timeout_secs
    )
    api_ctx.remote_fetch_params.max_retries = context.remote_max_retries
    api_ctx.remote_fetch_params.retry_backoff = context.remote_backoff_factor
    api_ctx.add_pip_as_python_dependency = context.add_pip_as_python_dependency
    api_ctx.use_only_tar_bz2 = context.use_only_tar_bz2

    if context.channel_priority is ChannelPriority.STRICT:
        api_ctx.channel_priority = api.ChannelPriority.kStrict
    elif context.channel_priority is ChannelPriority.FLEXIBLE:
        api_ctx.channel_priority = api.ChannelPriority.kFlexible
    elif context.channel_priority is ChannelPriority.DISABLED:
        api_ctx.channel_priority = api.ChannelPriority.kDisabled


def to_conda_channel(channel, platform):
    if channel.scheme == "file":
        return CondaChannel.from_value(
            channel.platform_url(platform, with_credentials=False)
        )

    return CondaChannel(
        channel.scheme,
        channel.auth,
        channel.location,
        channel.token,
        channel.name,
        platform,
        channel.package_filename,
    )


def to_package_record_from_subjson(entry, pkg, jsn_string):
    channel_url = entry["url"]
    info = json.loads(jsn_string)
    info["fn"] = pkg
    info["channel"] = to_conda_channel(entry["channel"], entry["platform"])
    info["url"] = join_url(channel_url, pkg)
    if not info.get("subdir"):
        info["subdir"] = entry["platform"]
    package_record = PackageRecord(**info)
    return package_record


def get_installed_packages(prefix, show_channel_urls=None):
    result = {"packages": {}}

    # Currently, we need to have pip interop disabled :/
    installed = {
        rec: rec for rec in PrefixData(prefix, pip_interop_enabled=False).iter_records()
    }

    # add virtual packages as installed packages
    # they are packages installed on the system that conda can do nothing
    # about (e.g. glibc)
    # if another version is needed, installation just fails
    # they don't exist anywhere (they start with __)
    _supplement_index_with_system(installed)
    installed = list(installed)

    for prec in installed:
        json_rec = prec.dist_fields_dump()
        json_rec["depends"] = prec.depends
        json_rec["constrains"] = prec.constrains
        json_rec["build"] = prec.build
        result["packages"][prec.fn] = json_rec

    return installed, result


installed_pkg_recs = None


def get_installed_jsonfile(prefix):
    global installed_pkg_recs
    installed_pkg_recs, output = get_installed_packages(prefix, show_channel_urls=True)
    installed_json_f = tempfile.NamedTemporaryFile("w", delete=False)
    installed_json_f.write(json_dump(output))
    installed_json_f.flush()
    return installed_json_f, installed_pkg_recs


def compute_final_precs(
    prefix_records, to_link, to_unlink, installed_pkg_recs, index=None
):
    if index is None:
        index = []
    to_link_records, to_unlink_records = [], []

    final_precs = IndexedSet(prefix_records)

    lookup_dict = {}
    for _, entry in index:
        lookup_dict[
            entry["channel"].platform_url(entry["platform"], with_credentials=False)
        ] = entry

    i_rec: PackageRecord
    for _, pkg in to_unlink:
        for i_rec in installed_pkg_recs:
            if i_rec.fn == pkg:
                try:
                    final_precs.remove(i_rec)
                    to_unlink_records.append(i_rec)
                except KeyError:
                    # virtual packages cannot be unlinked as they do not exist
                    if i_rec.package_type == "virtual_system":
                        continue
                    raise
                break
        else:
            print(f"No package record found for {pkg}!", file=sys.stderr)

    for c, pkg, jsn_s in to_link:
        if c.startswith("file://"):
            # The conda functions (specifically remove_auth) assume the input
            # is a url; a file uri on windows with a drive letter messes them
            # up.
            key = c
        else:
            key = split_anaconda_token(remove_auth(c))[0]
        if key not in lookup_dict:
            raise ValueError("missing key {} in channels: {}".format(key, lookup_dict))
        sdir = lookup_dict[key]

        rec = to_package_record_from_subjson(sdir, pkg, jsn_s)
        for ipkg in installed_pkg_recs:
            if ipkg.name == rec.name:
                rec.noarch = ipkg.noarch

        # virtual packages cannot be linked as they do not exist
        if rec.package_type == "virtual_system":
            continue

        final_precs.add(rec)
        to_link_records.append(rec)

    return IndexedSet(PrefixGraph(final_precs).graph)


def to_txn_precs(
    specs_to_add,
    specs_to_remove,
    prefix,
    final_precs,
):
    unlink_precs, link_precs = diff_for_unlink_link_precs(
        prefix,
        final_precs=final_precs,
        specs_to_add=specs_to_add,
        force_reinstall=context.force_reinstall,
    )

    pref_setup = PrefixSetup(
        target_prefix=prefix,
        unlink_precs=unlink_precs,
        link_precs=link_precs,
        remove_specs=specs_to_remove,
        update_specs=specs_to_add,
        neutered_specs=(),
    )

    conda_transaction = UnlinkLinkTransaction(pref_setup)
    return conda_transaction


def to_txn(
    specs_to_add,
    specs_to_remove,
    prefix,
    to_link,
    to_unlink,
    installed_pkg_recs,
    index=None,
):
    prefix_data = PrefixData(prefix)
    final_precs = compute_final_precs(
        prefix_data.iter_records(),
        to_link,
        to_unlink,
        installed_pkg_recs,
        index,
    )
    return to_txn_precs(
        specs_to_add,
        specs_to_remove,
        prefix,
        final_precs,
    )


def print_activate(env_name_or_prefix):  # pragma: no cover
    if not context.quiet and not context.json:
        message = (
            "\nTo activate this environment, use\n\n"
            f"     $ mamba activate {env_name_or_prefix}\n\n"
            "To deactivate an active environment, use\n\n"
            "     $ mamba deactivate\n"
        )
        print(message)  # TODO: use logger
