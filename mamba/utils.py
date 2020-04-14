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
              repodata_fn="repodata.json.bz2"):
    real_urls = calculate_channel_urls(channel_urls, prepend, platform, use_local)
    check_whitelist(real_urls)

    dlist = api.DownloadTargetList()

    sddata = []
    index = []
    for idx, url in enumerate(real_urls):
        channel = Channel(url)
        # fsd = FastSubdirData(, idx, repodata_fn)
        # index.append(fsd)

        full_url = channel.url(with_credentials=True) + '/' + repodata_fn

        full_path_cache= os.path.join(
            create_cache_dir(),
            cache_fn_url(full_url, repodata_fn))

        sd = api.SubdirData(channel.name + '/' + channel.subdir,
                            full_url,
                            full_path_cache)
        index.append((sd, channel))
        sd.load()
        dlist.add(sd)

    is_downloaded = dlist.download(True)

    if not is_downloaded:
        raise RuntimeError("Error downloading repodata.")

    return index

# def get_index(channel_urls=(), prepend=True, platform=None,
#               use_local=False, use_cache=False, unknown=None, prefix=None,
#               repodata_fn="repodata.json"):
#     real_urls = calculate_channel_urls(channel_urls, prepend, platform, use_local)
#     check_whitelist(real_urls)
#     result = get_env_index(real_urls, repodata_fn)
#     return result

# def get_env_index(channel_urls, repodata_fn="repodata.json"):
#     threads = []
#     result = []
#     sddata = [FastSubdirData(Channel(x), idx, repodata_fn) for idx, x in enumerate(channel_urls)]
#     for sd in sddata:
#         t = threading.Thread(target=load_channel, args=(sd, result))
#         threads.append(t)
#         t.start()

#     for t in threads:
#         t.join()

#     result = sorted(result, key=lambda x: x.channel_idx)
#     return result

def to_package_record_from_subjson(channel, pkg, jsn_string):
    channel = channel
    channel_url = channel.url(with_credentials=True)
    info = json.loads(jsn_string)
    info['fn'] = pkg
    info['channel'] = channel
    info['url'] = join_url(channel_url, pkg)
    package_record = PackageRecord(**info)
    return package_record
