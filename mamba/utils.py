# -*- coding: utf-8 -*-
# Copyright (C) 2019, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

from __future__ import absolute_import, division, print_function, unicode_literals

from .FastSubdirData import FastSubdirData
from conda.models.channel import Channel, prioritize_channels
from conda.core.index import calculate_channel_urls, check_whitelist #, get_index
from conda.models.records import PackageRecord
from conda.models.enums import PackageType
from conda.common.url import join_url
from conda.base.context import context

import threading
import json
import os

def load_channel(subdir_data, result_container):
    if not context.quiet:
        print("Getting ", subdir_data.channel.name, subdir_data.channel.platform)
    return result_container.append(subdir_data.load())

def get_index(channel_urls=(), prepend=True, platform=None,
              use_local=False, use_cache=False, unknown=None, prefix=None,
              repodata_fn="repodata.json"):
    real_urls = calculate_channel_urls(channel_urls, prepend, platform, use_local)
    check_whitelist(real_urls)
    result = get_env_index(real_urls, repodata_fn)
    return result

def get_env_index(channel_urls, repodata_fn="repodata.json"):
    threads = []
    result = []
    sddata = [FastSubdirData(Channel(x), idx, repodata_fn) for idx, x in enumerate(channel_urls)]
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

def _make_virtual_package(name, version=None):
    return PackageRecord(
            package_type=PackageType.VIRTUAL_SYSTEM,
            name=name,
            version=version or '0',
            build='0',
            channel='@',
            subdir=context.subdir,
            md5="12345678901234567890123456789012",
            build_number=0,
            fn=name,
    )

def _supplement_index_with_system(index):
    cuda_version = context.cuda_version
    if cuda_version is not None:
        rec = _make_virtual_package('__cuda', cuda_version)
        index.append(rec)

    dist_name, dist_version = context.os_distribution_name_version
    if dist_name == 'OSX':
        dist_version = os.environ.get('CONDA_OVERRIDE_OSX', dist_version)
        if len(dist_version) > 0:
            rec = _make_virtual_package('__osx', dist_version)
            index.append(rec)

    libc_family, libc_version = context.libc_family_version
    if libc_family and libc_version:
        libc_version = os.getenv("CONDA_OVERRIDE_{}".format(libc_family.upper()), libc_version)
        rec = _make_virtual_package('__' + libc_family, libc_version)
        index.append(rec)
