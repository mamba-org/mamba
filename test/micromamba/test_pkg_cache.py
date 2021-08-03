import json
import os
import platform
import random
import shutil
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

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    stat_xf = None
    stat_orig = None
    pkg_name = None
    orig_file_path = None

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestPkgCache.root_prefix
        os.environ["CONDA_PREFIX"] = TestPkgCache.prefix

        if "CONDA_PKGS_DIRS" in os.environ:
            os.pop("CONDA_PKGS_DIRS")

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestPkgCache.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestPkgCache.current_prefix

    @classmethod
    def setup(cls):
        res = create("-n", TestPkgCache.env_name, "xtensor", "--json", no_dry_run=True)

        xf = get_env(TestPkgCache.env_name, xtensor_hpp)
        assert xf.exists()
        TestPkgCache.stat_xf = xf.stat()

        TestPkgCache.pkg_name = get_concrete_pkg(res, "xtensor")
        TestPkgCache.orig_file_path = get_pkg(TestPkgCache.pkg_name, xtensor_hpp)
        TestPkgCache.stat_orig = TestPkgCache.orig_file_path.stat()

        assert TestPkgCache.stat_orig.st_dev == TestPkgCache.stat_xf.st_dev
        assert TestPkgCache.stat_orig.st_ino == TestPkgCache.stat_xf.st_ino

    @classmethod
    def teardown(cls):
        if Path(TestPkgCache.root_prefix).exists():
            shutil.rmtree(os.path.join(TestPkgCache.root_prefix, "envs"))

            extracted_pkg = os.path.join(
                TestPkgCache.root_prefix, "pkgs", TestPkgCache.pkg_name
            )
            if Path(extracted_pkg).exists():
                shutil.rmtree(extracted_pkg)

            tarball = os.path.join(
                TestPkgCache.root_prefix, "pkgs", TestPkgCache.pkg_name + ".tar.bz2"
            )
            if Path(tarball).exists():
                os.remove(tarball)

    def test_extracted_file_deleted(self):

        os.remove(TestPkgCache.orig_file_path)

        env = "x1"
        res = create("xtensor", "-n", env, "--json", no_dry_run=True)
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        assert TestPkgCache.stat_orig.st_dev == stat_xf.st_dev
        assert TestPkgCache.stat_orig.st_ino != stat_xf.st_ino

    @pytest.mark.parametrize("safety_checks", ["disabled", "warn", "enabled"])
    def test_extracted_file_corrupted(self, safety_checks):
        with open(TestPkgCache.orig_file_path, "w") as f:
            f.write("//corruption")

        env = "x1"
        res = create(
            "xtensor",
            "-n",
            env,
            "--json",
            "--safety-checks",
            safety_checks,
            no_dry_run=True,
        )
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        if safety_checks == "enabled":
            assert TestPkgCache.stat_orig.st_dev == stat_xf.st_dev
            assert TestPkgCache.stat_orig.st_ino != stat_xf.st_ino
        else:
            assert TestPkgCache.stat_orig.st_dev == stat_xf.st_dev
            assert TestPkgCache.stat_orig.st_ino == stat_xf.st_ino

    def test_tarball_deleted(self):
        os.remove(get_tarball(TestPkgCache.pkg_name))

        env = "x1"
        res = create("xtensor", "-n", env, "--json", no_dry_run=True)
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        assert not get_tarball(TestPkgCache.pkg_name).exists()
        assert TestPkgCache.stat_orig.st_dev == stat_xf.st_dev
        assert TestPkgCache.stat_orig.st_ino == stat_xf.st_ino

    def test_tarball_and_extracted_file_deleted(self):
        pkg_size = get_tarball(TestPkgCache.pkg_name).stat().st_size
        os.remove(TestPkgCache.orig_file_path)
        os.remove(get_tarball(TestPkgCache.pkg_name))

        env = "x1"
        res = create("xtensor", "-n", env, "--json", no_dry_run=True)
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        assert get_tarball(TestPkgCache.pkg_name).exists()
        assert pkg_size == get_tarball(TestPkgCache.pkg_name).stat().st_size
        assert TestPkgCache.stat_orig.st_dev == stat_xf.st_dev
        assert TestPkgCache.stat_orig.st_ino != stat_xf.st_ino

    def test_tarball_corrupted_and_extracted_file_deleted(self):
        pkg_size = get_tarball(TestPkgCache.pkg_name).stat().st_size
        os.remove(TestPkgCache.orig_file_path)
        os.remove(get_tarball(TestPkgCache.pkg_name))
        with open(get_tarball(TestPkgCache.pkg_name), "w") as f:
            f.write("")

        env = "x1"
        res = create("xtensor", "-n", env, "--json", no_dry_run=True)
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        assert get_tarball(TestPkgCache.pkg_name).exists()
        assert pkg_size == get_tarball(TestPkgCache.pkg_name).stat().st_size
        assert TestPkgCache.stat_orig.st_dev == stat_xf.st_dev
        assert TestPkgCache.stat_orig.st_ino != stat_xf.st_ino

    def test_extracted_file_corrupted_no_perm(self):
        with open(TestPkgCache.orig_file_path, "w") as f:
            f.write("//corruption")
        os.chmod(get_pkg(TestPkgCache.pkg_name), 0o500)

        env = "x1"

        create("xtensor", "-n", env, "--json", no_dry_run=True)

        os.chmod(get_pkg(TestPkgCache.pkg_name), 0o700)
