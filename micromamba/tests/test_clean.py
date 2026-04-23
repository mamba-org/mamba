import os

from .helpers import *  # noqa: F403
from . import helpers


def test_clean_all_removes_shard_cache(tmp_home, tmp_root_prefix):
    cache_home = tmp_home / ".cache"
    os.environ["XDG_CACHE_HOME"] = str(cache_home)

    shard_cache_dir = cache_home / "conda" / "pkgs" / "cache" / "shards"
    shard_cache_dir.mkdir(parents=True, exist_ok=True)
    shard_file = shard_cache_dir / "dummy.msgpack.zst"
    shard_file.write_bytes(b"dummy")

    assert shard_file.exists()

    helpers.clean("--all", "--no-rc", no_dry_run=True)

    assert not shard_cache_dir.exists()
