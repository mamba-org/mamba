# -*- coding: utf-8 -*-
# Copyright (C) 2012 Anaconda, Inc
# SPDX-License-Identifier: BSD-3-Clause
from __future__ import absolute_import, division, print_function, unicode_literals

import bz2
import sys, os
from collections import defaultdict
from contextlib import closing
from errno import EACCES, ENODEV, EPERM
from genericpath import getmtime, isfile
import hashlib
import json
from logging import DEBUG, getLogger
from mmap import ACCESS_READ, mmap
from os.path import dirname, isdir, join, splitext
import re
from time import time
import warnings
from io import open as io_open

from conda import CondaError
from conda._vendor.auxlib.ish import dals
from conda._vendor.auxlib.logz import stringify
from conda._vendor.toolz import concat, take
from conda.base.constants import CONDA_HOMEPAGE_URL
from conda.base.context import context
from conda.common.compat import (ensure_binary, ensure_text_type, ensure_unicode, iteritems,
                             string_types, text_type, with_metaclass)
from conda.common.io import ThreadLimitedThreadPoolExecutor, as_completed
from conda.common.url import join_url, maybe_unquote
from conda.core.package_cache_data import PackageCacheData
from conda.exceptions import (CondaDependencyError, CondaHTTPError, CondaUpgradeError,
                          NotWritableError, UnavailableInvalidChannel)
from conda.gateways.connection import (ConnectionError, HTTPError, InsecureRequestWarning,
                                   InvalidSchema, SSLError)
from conda.gateways.connection.session import CondaSession
from conda.gateways.disk import mkdir_p, mkdir_p_sudo_safe
from conda.gateways.disk.delete import rm_rf
from conda.gateways.disk.update import touch
from conda.models.channel import Channel, all_channel_urls
from conda.models.match_spec import MatchSpec
from conda.models.records import PackageRecord
from conda.core.subdir_data import *

log = getLogger(__name__)
stderrlog = getLogger('conda.stderrlog')

REPODATA_PICKLE_VERSION = 28
MAX_REPODATA_VERSION = 1
REPODATA_HEADER_RE = b'"(_etag|_mod|_cache_control)":[ ]?"(.*?[^\\\\])"[,\}\s]'  # NOQA


class SubdirDataType(type):

    def __call__(cls, channel):
        assert channel.subdir
        assert not channel.package_filename
        assert type(channel) is Channel
        cache_key = channel.url(with_credentials=True)
        if not cache_key.startswith('file://') and cache_key in FastSubdirData._cache_:
            return FastSubdirData._cache_[cache_key]

        subdir_data_instance = super(SubdirDataType, cls).__call__(channel)
        FastSubdirData._cache_[cache_key] = subdir_data_instance
        return subdir_data_instance


@with_metaclass(SubdirDataType)
class FastSubdirData(object):
    _cache_ = {}

    def __init__(self, channel):
        assert channel.subdir
        if channel.package_filename:
            parts = channel.dump()
            del parts['package_filename']
            channel = Channel(**parts)
        self.channel = channel
        self.url_w_subdir = self.channel.url(with_credentials=False)
        self.url_w_credentials = self.channel.url(with_credentials=True)
        self.cache_path_base = join(create_cache_dir(),
                                    splitext(cache_fn_url(self.url_w_credentials))[0])
        self._loaded = False
        # if the cache doesn't change, this stays False
        self.cache_content_changed = False

    def reload(self):
        self._loaded = False
        self.load()
        return self

    @property
    def cache_path_json(self):
        return self.cache_path_base + '.json'

    @property
    def cache_path_solv(self):
        return self.cache_path_base + '.solv'

    def get_loaded_file_path(self):
        if sys.platform == 'win32':
            return self.cache_path_json

        if os.path.exists(self.cache_path_solv) and \
           self.cache_content_changed == False and \
           os.path.getmtime(self.cache_path_solv) > os.path.getmtime(self.cache_path_json):
            return self.cache_path_solv

        return self.cache_path_json

    def load(self):
        self._load()
        self._loaded = True
        return self

    def iter_records(self):
        if not self._loaded:
            self.load()
        return iter(self._package_records)

    def _load(self):
        try:
            mtime = getmtime(self.cache_path_json)
        except (IOError, OSError):
            log.debug("No local cache found for %s at %s", self.url_w_subdir, self.cache_path_json)
            if context.use_index_cache or (context.offline
                                           and not self.url_w_subdir.startswith('file://')):
                log.debug("Using cached data for %s at %s forced. Returning empty repodata.",
                          self.url_w_subdir, self.cache_path_json)
                return {
                    '_package_records': (),
                    '_names_index': defaultdict(list),
                    '_track_features_index': defaultdict(list),
                }
            else:
                mod_etag_headers = {}
        else:
            mod_etag_headers = read_mod_and_etag(self.cache_path_json)

            if context.use_index_cache:
                log.debug("Using cached repodata for %s at %s because use_cache=True",
                          self.url_w_subdir, self.cache_path_json)
                return

            if context.local_repodata_ttl > 1:
                max_age = context.local_repodata_ttl
            elif context.local_repodata_ttl == 1:
                max_age = get_cache_control_max_age(mod_etag_headers.get('_cache_control', ''))
            else:
                max_age = 0

            timeout = mtime + max_age - time()
            if (timeout > 0 or context.offline) and not self.url_w_subdir.startswith('file://'):
                log.debug("Using cached repodata for %s at %s. Timeout in %d sec",
                          self.url_w_subdir, self.cache_path_json, timeout)
                return

            log.debug("Local cache timed out for %s at %s",
                      self.url_w_subdir, self.cache_path_json)

        try:
            raw_repodata_str = fetch_repodata_remote_request(self.url_w_credentials,
                                                             mod_etag_headers.get('_etag'),
                                                             mod_etag_headers.get('_mod'))
        except Response304ContentUnchanged:
            log.debug("304 NOT MODIFIED for '%s'. Updating mtime and loading from disk",
                      self.url_w_subdir)
            # Do not touch here, so we can compare the creation date of solv vs. json file
            # for mamba, and regenerate the solv file if updated from conda.
            # touch(self.cache_path_json)
            return

        else:
            if not isdir(dirname(self.cache_path_json)):
                mkdir_p(dirname(self.cache_path_json))
            try:
                with io_open(self.cache_path_json, 'w') as fh:
                    fh.write(raw_repodata_str or '{}')
                    self.cache_content_changed = True
            except (IOError, OSError) as e:
                if e.errno in (EACCES, EPERM):
                    raise NotWritableError(self.cache_path_json, e.errno, caused_by=e)
                else:
                    raise
