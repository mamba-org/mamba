# -*- coding: utf-8 -*-
# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

from __future__ import absolute_import, division, print_function, unicode_literals

from conda.models.channel import Channel, prioritize_channels
from conda.core.index import calculate_channel_urls, check_whitelist #, get_index
from conda.models.records import PackageRecord
from conda.models.enums import PackageType
from conda.common.url import join_url
from conda.base.context import context
from conda.core.subdir_data import cache_fn_url, create_cache_dir

import threading
import json
import os

import mamba.mamba_api as api

def load_channel(subdir_data, result_container):
    if not context.quiet:
        print("Getting ", subdir_data.channel.name, subdir_data.channel.platform)
    return result_container.append(subdir_data.load())

def get_index(channel_urls=(), prepend=True, platform=None,
              use_local=False, use_cache=False, unknown=None, prefix=None,
              repodata_fn="repodata.json"):

    real_urls = calculate_channel_urls(channel_urls, prepend, platform, use_local)
    check_whitelist(real_urls)

    dlist = api.DownloadTargetList()

    sddata = []
    index = []
    for idx, url in enumerate(real_urls):
        channel = Channel(url)

        full_url = channel.url(with_credentials=True) + '/' + repodata_fn
        full_path_cache = os.path.join(
            create_cache_dir(),
            cache_fn_url(full_url, repodata_fn))

        sd = api.SubdirData(channel.name + '/' + channel.subdir,
                            full_url,
                            full_path_cache)

        sd.load()
        index.append((sd, channel))
        dlist.add(sd)

    is_downloaded = dlist.download(True)

    if not is_downloaded:
        raise RuntimeError("Error downloading repodata.")

    return index

def init_api_context(use_mamba_experimental=False):
    api_ctx = api.Context()

    api_ctx.json = context.json
    api_ctx.dry_run = context.dry_run
    if context.json:
        context.always_yes = True
        context.quiet = True
        if use_mamba_experimental:
            context.json = False
            # context.dry_run = False
    api_ctx.set_verbosity(context.verbosity)
    api_ctx.quiet = context.quiet
    api_ctx.offline = context.offline
    api_ctx.local_repodata_ttl = context.local_repodata_ttl
    api_ctx.use_index_cache = context.use_index_cache
    api_ctx.always_yes = context.always_yes
    api_ctx.channels = context.channels

    if context.ssl_verify == False:
        api_ctx.ssl_verify = "<false>"
    elif context.ssl_verify is not True:
        api_ctx.ssl_verify = context.ssl_verify
    api_ctx.target_prefix = context.target_prefix

    api_ctx.read_timeout_secs = int(round(context.remote_read_timeout_secs))
    api_ctx.connect_timeout_secs = int(round(context.remote_connect_timeout_secs))
    api_ctx.max_retries = context.remote_max_retries
    api_ctx.retry_backoff = context.remote_backoff_factor

def to_package_record_from_subjson(channel, pkg, jsn_string):
    channel = channel
    channel_url = channel.url(with_credentials=True)
    info = json.loads(jsn_string)
    info['fn'] = pkg
    info['channel'] = channel
    info['url'] = join_url(channel_url, pkg)
    package_record = PackageRecord(**info)
    return package_record
