# -*- coding: utf-8 -*-
# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

from __future__ import absolute_import, division, print_function, unicode_literals

from .FastSubdirData import FastSubdirData
from conda.models.channel import Channel, prioritize_channels
from conda.core.index import calculate_channel_urls, check_whitelist #, get_index
from conda.models.records import PackageRecord
from conda.common.url import join_url
from conda.base.context import context

import threading
import json

def load_channel(subdir_data, result_container):
    if not context.quiet:
        print("Getting ", subdir_data.channel.name, subdir_data.channel.platform)
    return result_container.append(subdir_data.load())

def get_index(channel_urls=(), prepend=True, platform=None,
              use_local=False, use_cache=False, unknown=None, prefix=None):
    real_urls = calculate_channel_urls(channel_urls, prepend, platform, use_local)
    check_whitelist(real_urls)
    threads = []
    result = []
    sddata = [FastSubdirData(Channel(x), idx) for idx, x in enumerate(real_urls)]
    for sd in sddata:
        t = threading.Thread(target=load_channel, args=(sd, result))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    result = sorted(result, key=lambda x: x.channel_idx)
    return result

def get_env_index(channel_urls):
    threads = []
    result = []
    sddata = [FastSubdirData(Channel(x), idx) for idx, x in enumerate(channel_urls)]
    for sd in sddata:
        t = threading.Thread(target=load_channel, args=(sd, result))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    result = sorted(result, key=lambda x: x.channel_idx)
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