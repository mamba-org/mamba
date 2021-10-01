import json
import os
import platform
import random
import shutil
import stat
import string
import subprocess
from pathlib import Path

import pytest

from .helpers import *

if platform.system() == "Windows":
    xtensor_hpp = "Library/include/xtensor/xtensor.hpp"
else:
    xtensor_hpp = "include/xtensor/xtensor.hpp"


class TestPkgCache:

    current_root_prefix = os.environ.get("MAMBA_ROOT_PREFIX", "")
    current_prefix = os.environ.get("CONDA_PREFIX", "")
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    stat_xf = None
    stat_orig = None
    pkg_name = None
    orig_file_path = None

    @classmethod
    @pytest.fixture
    def cache(cls, existing_cache, test_pkg):
        cache = Path(os.path.expanduser(os.path.join("~", "cache" + random_string())))
        os.makedirs(cache)
        link_dir(cache, existing_cache)

        os.environ["CONDA_PKGS_DIRS"] = str(cache)

        yield cache

        if cache.exists():
            os.chmod(cache, 0o700)
            os.chmod(cache / test_pkg, 0o700)
            os.chmod(cache / test_pkg / xtensor_hpp, 0o700)
            rmtree(cache)

    @classmethod
    @pytest.fixture
    def cached_file(cls, cache, test_pkg):
        return cache / test_pkg / xtensor_hpp

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestPkgCache.root_prefix
        os.environ["CONDA_PREFIX"] = TestPkgCache.prefix

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestPkgCache.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestPkgCache.current_prefix

        if "CONDA_PKGS_DIRS" in os.environ:
            os.environ.pop("CONDA_PKGS_DIRS")

        if Path(TestPkgCache.root_prefix).exists():
            rmtree(TestPkgCache.root_prefix)

    @classmethod
    def teardown(cls):
        envs_dir = os.path.join(TestPkgCache.root_prefix, "envs")
        if Path(envs_dir).exists():
            rmtree(envs_dir)

    def test_extracted_file_deleted(self, cached_file):
        old_ino = cached_file.stat().st_ino
        os.remove(cached_file)

        env = "x1"
        create("xtensor", "-n", env, "--json", no_dry_run=True)

        linked_file = get_env(env, xtensor_hpp)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert cached_file.stat().st_dev == linked_file_stats.st_dev
        assert cached_file.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    @pytest.mark.parametrize("safety_checks", ["disabled", "warn", "enabled"])
    def test_extracted_file_corrupted(self, safety_checks, cached_file):
        old_ino = cached_file.stat().st_ino

        with open(cached_file, "w") as f:
            f.write("//corruption")

        env = "x1"
        create(
            "xtensor",
            "-n",
            env,
            "--json",
            "--safety-checks",
            safety_checks,
            no_dry_run=True,
        )

        linked_file = get_env(env, xtensor_hpp)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert cached_file.stat().st_dev == linked_file_stats.st_dev
        assert cached_file.stat().st_ino == linked_file_stats.st_ino

        if safety_checks == "enabled":
            assert old_ino != linked_file_stats.st_ino
        else:
            assert old_ino == linked_file_stats.st_ino

    def test_tarball_deleted(self, cached_file, test_pkg, cache):
        tarball = cache / Path(str(test_pkg) + ".tar.bz2")
        os.remove(tarball)

        env = "x1"
        create("xtensor", "-n", env, "--json", no_dry_run=True)

        linked_file = get_env(env, xtensor_hpp)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert not (tarball).exists()
        assert cached_file.stat().st_dev == linked_file_stats.st_dev
        assert cached_file.stat().st_ino == linked_file_stats.st_ino

    def test_tarball_and_extracted_file_deleted(self, cache, test_pkg, cached_file):
        tarball = cache / Path(str(test_pkg) + ".tar.bz2")
        tarball_size = tarball.stat().st_size
        old_ino = cached_file.stat().st_ino
        os.remove(cached_file)
        os.remove(tarball)

        env = "x1"
        create("xtensor", "-n", env, "--json", no_dry_run=True)

        linked_file = get_env(env, xtensor_hpp)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tarball.exists()
        assert tarball_size == tarball.stat().st_size
        assert cached_file.stat().st_dev == linked_file_stats.st_dev
        assert cached_file.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    def test_tarball_corrupted_and_extracted_file_deleted(
        self, cache, test_pkg, cached_file
    ):
        tarball = cache / Path(str(test_pkg) + ".tar.bz2")
        tarball_size = tarball.stat().st_size
        old_ino = cached_file.stat().st_ino
        os.remove(cached_file)
        os.remove(tarball)
        with open(tarball, "w") as f:
            f.write("")

        env = "x1"
        create("xtensor", "-n", env, "--json", no_dry_run=True)

        linked_file = get_env(env, xtensor_hpp)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tarball.exists()
        assert tarball_size == tarball.stat().st_size
        assert cached_file.stat().st_dev == linked_file_stats.st_dev
        assert cached_file.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    def test_extracted_file_corrupted_no_perm(self, cache, cached_file, test_pkg):
        with open(cached_file, "w") as f:
            f.write("//corruption")
        recursive_chmod(cache / test_pkg, 0o500)

        env = "x1"
        cmd_args = ("xtensor", "-n", env, "--json", "-vv")

        create(*cmd_args, no_dry_run=True)


class TestMultiplePkgCaches:

    current_root_prefix = os.environ.get("MAMBA_ROOT_PREFIX", "")
    current_prefix = os.environ.get("CONDA_PREFIX", "")
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @staticmethod
    @pytest.fixture
    def cache1(existing_cache, first_cache_is_writable):
        cache = Path(os.path.expanduser(os.path.join("~", "cache" + random_string())))
        os.makedirs(cache)

        if first_cache_is_writable:
            link_dir(cache, existing_cache)
        else:
            recursive_chmod(cache, 0o500)

        yield cache

        if cache.exists():
            rmtree(cache)

    @staticmethod
    @pytest.fixture
    def cache2(existing_cache, first_cache_is_writable):
        cache = Path(os.path.expanduser(os.path.join("~", "cache" + random_string())))
        os.makedirs(cache)
        link_dir(cache, existing_cache)

        yield cache

        if cache.exists():
            rmtree(cache)

    @staticmethod
    @pytest.fixture
    def used_cache(cache1, cache2, first_cache_is_writable):
        if first_cache_is_writable:
            yield cache1
        else:
            yield cache2

    @staticmethod
    @pytest.fixture
    def unused_cache(cache1, cache2, first_cache_is_writable):
        if first_cache_is_writable:
            yield cache2
        else:
            yield cache1

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestMultiplePkgCaches.root_prefix
        os.environ["CONDA_PREFIX"] = TestMultiplePkgCaches.prefix
        if "CONDA_PKGS_DIRS" in os.environ:
            os.environ.pop("CONDA_PKGS_DIRS")

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestMultiplePkgCaches.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestMultiplePkgCaches.current_prefix

        if Path(TestMultiplePkgCaches.root_prefix).exists():
            rmtree(TestMultiplePkgCaches.root_prefix)

    @classmethod
    def teardown(cls):
        if "CONDA_PKGS_DIRS" in os.environ:
            os.environ.pop("CONDA_PKGS_DIRS")

    @pytest.mark.parametrize("cache", (pytest.lazy_fixture(("cache1", "cache2"))))
    def test_different_caches(self, cache):
        os.environ["CONDA_PKGS_DIRS"] = f"{cache}"

        env_name = TestMultiplePkgCaches.env_name
        res = create("-n", env_name, "xtensor", "-v", "--json", no_dry_run=True)

        linked_file = get_env(env_name, xtensor_hpp)
        assert linked_file.exists()

        pkg_name = get_concrete_pkg(res, "xtensor")
        cache_file = cache / pkg_name / xtensor_hpp

        assert cache_file.exists()

        assert linked_file.stat().st_dev == cache_file.stat().st_dev
        assert linked_file.stat().st_ino == cache_file.stat().st_ino

    @pytest.mark.parametrize("first_is_writable", (False, True))
    def test_first_writable(
        self, first_is_writable, cache1, cache2, used_cache, unused_cache
    ):
        os.environ["CONDA_PKGS_DIRS"] = f"{cache1},{cache2}"
        env_name = TestMultiplePkgCaches.env_name

        res = create("-n", env_name, "xtensor", "--json", no_dry_run=True,)

        linked_file = get_env(env_name, xtensor_hpp)
        assert linked_file.exists()

        pkg_name = get_concrete_pkg(res, "xtensor")
        cache_file = used_cache / pkg_name / xtensor_hpp

        assert cache_file.exists()

        assert linked_file.stat().st_dev == cache_file.stat().st_dev
        assert linked_file.stat().st_ino == cache_file.stat().st_ino

    def test_extracted_tarball_only_in_non_writable_cache(
        self, cache1, cache2, test_pkg, repodata_files
    ):
        tarball = cache1 / Path(str(test_pkg) + ".tar.bz2")
        rmtree(tarball)
        # this will chmod 700 the hardlinks and have to be done before chmod cache1
        rmtree(cache2)
        recursive_chmod(cache1, 0o500)

        os.environ["CONDA_PKGS_DIRS"] = f"{cache1},{cache2}"
        env_name = TestMultiplePkgCaches.env_name

        create("-n", env_name, "xtensor", "--json", no_dry_run=True)

        linked_file = get_env(env_name, xtensor_hpp)
        assert linked_file.exists()

        non_writable_cache_file = cache1 / test_pkg / xtensor_hpp
        writable_cache_file = cache2 / test_pkg / xtensor_hpp

        # check repodata files
        for f in repodata_files:
            for ext in ["json", "solv"]:
                assert (cache1 / "cache" / (f + "." + ext)).exists()
                assert not (cache2 / "cache" / (f + "." + ext)).exists()

        # check tarballs
        assert not (cache1 / Path(str(test_pkg) + ".tar.bz2")).exists()
        assert not (cache2 / Path(str(test_pkg) + ".tar.bz2")).exists()

        # check extracted files
        assert non_writable_cache_file.exists()
        assert not writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == non_writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == non_writable_cache_file.stat().st_ino

    def test_expired_but_valid_repodata_in_non_writable_cache(
        self, cache1, cache2, test_pkg, repodata_files
    ):
        rmtree(cache2)
        recursive_chmod(cache1, 0o500)

        os.environ["CONDA_PKGS_DIRS"] = f"{cache1},{cache2}"
        env_name = TestMultiplePkgCaches.env_name

        create(
            "-n",
            env_name,
            "xtensor",
            "-vv",
            "--json",
            "--repodata-ttl=0",
            no_dry_run=True,
        )

        linked_file = get_env(env_name, xtensor_hpp)
        assert linked_file.exists()

        non_writable_cache_file = cache1 / test_pkg / xtensor_hpp
        writable_cache_file = cache2 / test_pkg / xtensor_hpp

        # check repodata files
        for f in repodata_files:
            for ext in ["json", "solv"]:
                assert (cache1 / "cache" / (f + "." + ext)).exists()
                assert (cache2 / "cache" / (f + "." + ext)).exists()

        # check tarballs
        assert (cache1 / Path(str(test_pkg) + ".tar.bz2")).exists()
        assert not (cache2 / Path(str(test_pkg) + ".tar.bz2")).exists()

        # check extracted dir
        assert (cache1 / test_pkg).exists()
        assert not (cache2 / test_pkg).exists()

        # check extracted files
        assert non_writable_cache_file.exists()
        assert not writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == non_writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == non_writable_cache_file.stat().st_ino
