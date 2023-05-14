# Copyright (C) 2020, QuantStack
# SPDX-License-Identifier: BSD-3-Clause

import json

from conda.base.context import context

import libmambapy as api
from mamba.utils import init_api_context, load_channels


def _repoquery(query_type, q, pool, fmt=api.QueryFormat.JSON):
    query = api.Query(pool)
    if query_type == "whoneeds":
        return query.whoneeds(q, fmt)
    if query_type == "depends":
        return query.depends(q, fmt)
    if query_type == "search":
        return query.find(q, fmt)


def create_pool(
    channels,
    platform,
    installed,
    repodata_fn="repodata.json",
    use_cache=True,
    use_local=False,
):
    if hasattr(context, "__initialized__") is False or context.__initialized__ is False:
        context.__init__()
        context.__initialized__ = True

    init_api_context()

    pool = api.Pool()
    repos = []

    if installed:
        prefix_data = api.PrefixData(context.target_prefix)
        repo = api.Repo(pool, prefix_data)
        repos.append(repo)

    if channels:
        repos = []
        load_channels(
            pool,
            channels,
            repos,
            prepend=False,
            platform=platform,
            use_cache=use_cache,
            repodata_fn=repodata_fn,
            use_local=use_local,
        )

    return pool


def search(query, pool=None):
    if not pool:
        pool = create_pool(["conda-forge"], "linux-64", False)
    res = _repoquery("search", query, pool)
    return json.loads(res)


def depends(query, pool=None):
    if not pool:
        pool = create_pool([], "linux-64", True)
    res = _repoquery("depends", query, pool)
    return json.loads(res)


def whoneeds(query, pool=None):
    if not pool:
        pool = create_pool([], "linux-64", True)
    res = _repoquery("whoneeds", query, pool)
    return json.loads(res)
