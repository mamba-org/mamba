import json

from conda.base.context import context

import mamba.mamba_api as api
from mamba.utils import get_index, init_api_context


def _repoquery(query_type, q, pool, fmt=api.QueryFormat.JSON):
    query = api.Query(pool)
    if query_type == "whoneeds":
        return query.whoneeds(q, fmt)
    if query_type == "depends":
        return query.depends(q, fmt)
    if query_type == "search":
        return query.find(q, fmt)


def create_context(channels, platform, installed):
    if hasattr(context, "__initialized__") is False or context.__initialized__ is False:
        context.__init__()
        context.__initialized__ = True

    prefix = None
    if installed is not False:
        prefix = installed

    init_api_context()

    pool = api.Pool()
    repos = []

    if installed:
        prefix_data = api.PrefixData(context.target_prefix)
        prefix_data.load()
        repo = api.Repo(pool, prefix_data)
        repos.append(repo)

    # todo use load_channels

    if channels:
        index = get_index(
            channel_urls=channels,
            prepend=False,  # this removes default
            platform=platform,
            use_local=False,
            use_cache=True,
            unknown=False,
            prefix=prefix,
        )

        for subdir, channel in index:
            if not subdir.loaded() and channel.platform != "noarch":
                # ignore non-loaded subdir if channel is != noarch
                continue

            repo = api.Repo(
                pool,
                str(channel),
                subdir.cache_path(),
                channel.url(with_credentials=True),
            )
            repo.set_priority(0, 0)
            repos.append(repo)

    return pool


def search(query, pool=None):
    if not pool:
        pool = create_context(["conda-forge"], "linux-64", False)
    res = _repoquery("search", query, pool)
    return json.loads(res)


def depends(query, pool=None):
    if not pool:
        pool = create_context([], "linux-64", True)
    res = _repoquery("depends", query, pool)
    return json.loads(res)


def whoneeds(query, pool=None):
    if not pool:
        pool = create_context([], "linux-64", True)
    res = _repoquery("whoneeds", query, pool)
    return json.loads(res)
