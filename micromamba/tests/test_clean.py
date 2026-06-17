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


def test_clean_all_removes_nested_package_cache_entries(tmp_home, tmp_root_prefix):
    pkgs_dir = tmp_home / "pkgs"
    os.environ["CONDA_PKGS_DIRS"] = str(pkgs_dir)

    channel_dir = pkgs_dir / "https" / "conda.anaconda.org" / "conda-forge" / "linux-64"
    channel_dir.mkdir(parents=True, exist_ok=True)

    tarball = channel_dir / "xtensor-0.24.7-h2acdbc0_0.conda"
    tarball.write_bytes(b"dummy")

    extracted = channel_dir / "xtensor-0.24.7-h2acdbc0_0"
    (extracted / "info").mkdir(parents=True, exist_ok=True)
    (extracted / "info" / "index.json").write_text("{}")

    assert tarball.exists()
    assert extracted.exists()

    helpers.clean("--all", "--no-rc", no_dry_run=True)

    assert not tarball.exists()
    assert not extracted.exists()


def test_clean_all_clears_all_cache_kinds(tmp_home, tmp_root_prefix):
    pkgs_dir = tmp_home / "pkgs"
    os.environ["CONDA_PKGS_DIRS"] = str(pkgs_dir)
    cache_home = tmp_home / ".cache"
    os.environ["XDG_CACHE_HOME"] = str(cache_home)

    index_cache_dir = pkgs_dir / "cache"
    index_cache_dir.mkdir(parents=True, exist_ok=True)
    index_cache_file = index_cache_dir / "abc123.json"
    index_cache_file.write_text("{}")

    channel_dir = pkgs_dir / "https" / "conda.anaconda.org" / "conda-forge" / "linux-64"
    channel_dir.mkdir(parents=True, exist_ok=True)
    tarball = channel_dir / "xtensor-0.24.7-h2acdbc0_0.conda"
    tarball.write_bytes(b"dummy")
    extracted = channel_dir / "xtensor-0.24.7-h2acdbc0_0"
    (extracted / "info").mkdir(parents=True, exist_ok=True)
    (extracted / "info" / "index.json").write_text("{}")

    user_shard_cache_dir = cache_home / "conda" / "pkgs" / "cache" / "shards"
    user_shard_cache_dir.mkdir(parents=True, exist_ok=True)
    user_shard_file = user_shard_cache_dir / "dummy.msgpack.zst"
    user_shard_file.write_bytes(b"dummy")
    user_channel_dir = (
        cache_home / "conda" / "pkgs" / "https" / "conda.anaconda.org" / "conda-forge" / "linux-64"
    )
    user_channel_dir.mkdir(parents=True, exist_ok=True)
    user_tarball = user_channel_dir / "usercache-1.0-0.conda"
    user_tarball.write_bytes(b"dummy")
    user_extracted = user_channel_dir / "usercache-1.0-0"
    (user_extracted / "info").mkdir(parents=True, exist_ok=True)
    (user_extracted / "info" / "index.json").write_text("{}")
    user_conda_other_cache_dir = cache_home / "conda" / "repodata"
    user_conda_other_cache_dir.mkdir(parents=True, exist_ok=True)
    user_conda_other_cache_file = user_conda_other_cache_dir / "repodata-state.json"
    user_conda_other_cache_file.write_text("{}")

    assert index_cache_file.exists()
    assert tarball.exists()
    assert extracted.exists()
    assert user_shard_file.exists()
    assert user_tarball.exists()
    assert user_extracted.exists()
    assert user_conda_other_cache_file.exists()

    helpers.clean("--all", "--no-rc", no_dry_run=True)

    assert not index_cache_dir.exists()
    assert not tarball.exists()
    assert not extracted.exists()
    assert not user_shard_cache_dir.exists()
    assert not user_tarball.exists()
    assert not user_extracted.exists()
    assert not (cache_home / "conda").exists()
