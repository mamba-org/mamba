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


class TestInstall:
    def test_empty(self):
        assert install().strip() == "Nothing to do."

    def test_already_installed(self):
        res = install("numpy", "--json", "--dry-run")
        keys = {"dry_run", "success", "prefix"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]

    def test_not_already_installed(self):
        res = install("bokeh", "--json", "--dry-run")
        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys == set(res.keys())
        assert res["success"]

    def test_dependencies(self):
        res = create("-n", random_string(), "xtensor", "--json", "--dry-run")
        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]

        action_keys = {"LINK", "PREFIX"}
        assert action_keys == set(res["actions"].keys())

        packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
        expected_packages = {"xtensor", "xtl"}
        assert expected_packages.issubset(packages)


if platform.system() == "Windows":
    xtensor_hpp = "Library/include/xtensor/xtensor.hpp"
else:
    xtensor_hpp = "include/xtensor/xtensor.hpp"


class TestLinking:
    def test_link(self):
        res = create("xtensor", "-n", "x1", "--json")
        xf = get_env("x1", xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)

        stat_orig = orig_file_path.stat()
        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino
        assert not xf.is_symlink()

        shutil.rmtree(get_env("x1"))

    def test_copy(self):
        env = "x2"
        res = create("xtensor", "-n", env, "--json", "--always-copy")
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)

        assert not xf.is_symlink()

        stat_orig = orig_file_path.stat()
        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino != stat_xf.st_ino

        shutil.rmtree(get_env(env))

    @pytest.mark.skipif(
        platform.system() == "Windows",
        reason="Softlinking needs admin privileges on win",
    )
    def test_always_softlink(self):
        env = "x3"
        res = create("xtensor", "-n", env, "--json", "--always-softlink")
        xf = get_env(env, xtensor_hpp)
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)

        stat_orig = orig_file_path.stat()
        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino
        assert xf.is_symlink()

        assert os.readlink(xf) == str(orig_file_path)

        shutil.rmtree(get_env(env))

    def test_cross_device(self):
        if platform.system() == "Linux":
            with open("/tmp/test", "w") as f:
                f.write("abc")
            p = Path("/tmp/test")
            dev_tmp = p.stat().st_dev

            check_file = get_pkg("test.xyz")
            with open(check_file, "w") as f:
                f.write("abc")
            chk_stat = check_file.stat()
            tmp_stat = p.stat()

            if tmp_stat.st_dev == chk_stat.st_dev:
                print("tmp is not a different device")
                return

            env = "/tmp/testenv"
            res = create("xtensor", "-p", "/tmp/testenv", "--json", "--always-copy")
            xf = Path(env + "/include/xtensor/xtensor.hpp")
            assert xf.exists()
            stat_xf = xf.stat()

            pkg_name = get_concrete_pkg(res, "xtensor")
            orig_file_path = get_pkg(pkg_name, "include/xtensor/xtensor.hpp")

            stat_orig = orig_file_path.stat()
            assert (
                stat_orig.st_dev != stat_xf.st_dev
                and stat_orig.st_ino != stat_xf.st_ino
            )
            assert not xf.is_symlink()

            shutil.rmtree("/tmp/testenv")
