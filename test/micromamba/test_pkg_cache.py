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
    def test_extracted_file_deleted(self):
        ref_env = "x"
        res = create("xtensor", "-n", ref_env, "--json")
        xf = get_env(ref_env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        os.remove(orig_file_path)

        env = "x1"
        res = create("xtensor", "-n", env, "--json")
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino != stat_xf.st_ino

        shutil.rmtree(get_env(ref_env))
        shutil.rmtree(get_env(env))

    @pytest.mark.parametrize("safety_checks", ["disabled", "warn", "enabled"])
    def test_extracted_file_corrupted(self, safety_checks):
        ref_env = "x"
        res = create(
            "xtensor", "-n", ref_env, "--json", "--safety-checks", safety_checks
        )
        xf = get_env(ref_env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        with open(orig_file_path, "w") as f:
            f.write("//corruption")

        env = "x1"
        res = create("xtensor", "-n", env, "--json", "--safety-checks", safety_checks)
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        if safety_checks == "enabled":
            assert (
                stat_orig.st_dev == stat_xf.st_dev
                and stat_orig.st_ino != stat_xf.st_ino
            )
        else:
            assert (
                stat_orig.st_dev == stat_xf.st_dev
                and stat_orig.st_ino == stat_xf.st_ino
            )

        os.remove(orig_file_path)
        shutil.rmtree(get_env(ref_env))
        shutil.rmtree(get_env(env))

    def test_tarball_deleted(self):
        ref_env = "x"
        res = create("xtensor", "-n", ref_env, "--json")
        xf = get_env(ref_env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        os.remove(get_tarball(pkg_name))

        env = "x1"
        res = create("xtensor", "-n", env, "--json")
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        assert not get_tarball(pkg_name).exists()
        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        shutil.rmtree(get_pkg(pkg_name))  # clean for other tests
        shutil.rmtree(get_env(ref_env))
        shutil.rmtree(get_env(env))

    def test_tarball_and_extracted_file_deleted(self):
        ref_env = "x"
        res = create("xtensor", "-n", ref_env, "--json")
        xf = get_env(ref_env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        pkg_size = get_tarball(pkg_name).stat().st_size
        os.remove(orig_file_path)
        os.remove(get_tarball(pkg_name))

        env = "x1"
        res = create("xtensor", "-n", env, "--json")
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        assert get_tarball(pkg_name).exists()
        assert pkg_size == get_tarball(pkg_name).stat().st_size
        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino != stat_xf.st_ino

        shutil.rmtree(get_env(ref_env))
        shutil.rmtree(get_env(env))

    def test_tarball_corrupted_and_extracted_file_deleted(self):
        ref_env = "x"
        res = create("xtensor", "-n", ref_env, "--json")
        xf = get_env(ref_env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        pkg_size = get_tarball(pkg_name).stat().st_size
        os.remove(orig_file_path)
        os.remove(get_tarball(pkg_name))
        with open(get_tarball(pkg_name), "w") as f:
            f.write("")

        env = "x1"
        res = create("xtensor", "-n", env, "--json")
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        assert get_tarball(pkg_name).exists()
        assert pkg_size == get_tarball(pkg_name).stat().st_size
        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino != stat_xf.st_ino

        shutil.rmtree(get_env(ref_env))
        shutil.rmtree(get_env(env))

    def test_extracted_file_corrupted_no_perm(self):
        ref_env = "x"
        res = create("xtensor", "-n", ref_env, "--json")
        xf = get_env(ref_env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        with open(orig_file_path, "w") as f:
            f.write("//corruption")
        os.chmod(get_pkg(pkg_name), 0o500)

        env = "x1"

        create("xtensor", "-n", env, "--json")

        os.chmod(get_pkg(pkg_name), 0o755)
        os.remove(orig_file_path)
        shutil.rmtree(get_env(ref_env))
        shutil.rmtree(get_env(env))
