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


class TestLinking:
    def test_link(self):
        res = create("xtensor", "-n", "x1", "--json", no_dry_run=True)
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
        res = create("xtensor", "-n", env, "--json", "--always-copy", no_dry_run=True)
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
        res = create(
            "xtensor", "-n", env, "--json", "--always-softlink", no_dry_run=True
        )
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

    @pytest.mark.parametrize("allow_softlinks", [True, False])
    @pytest.mark.parametrize("always_copy", [True, False])
    def test_cross_device(self, allow_softlinks, always_copy):
        if platform.system() != "Linux":
            pytest.skip("o/s is not linux")

        with open("/tmp/test", "w") as f:
            f.write("abc")
        p = Path("/tmp/test")
        dev_tmp = p.stat().st_dev

        check_file = get_pkg("test.xyz")
        with open(check_file, "w") as f:
            f.write("abc")
        chk_stat = check_file.stat()
        tmp_stat = p.stat()

        os.remove("/tmp/test")

        if tmp_stat.st_dev == chk_stat.st_dev:
            pytest.skip("/tmp is on the same file system as root_prefix")

        env = "/tmp/testenv"

        create_args = ["xtensor", "-p", "/tmp/testenv", "--json"]
        if allow_softlinks:
            create_args.append("--allow-softlinks")
        if always_copy:
            create_args.append("--always-copy")
        res = create(*create_args, no_dry_run=True)

        xf = Path(env) / xtensor_hpp
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)

        stat_orig = orig_file_path.stat()
        assert stat_orig.st_dev != stat_xf.st_dev and stat_orig.st_ino != stat_xf.st_ino
        assert xf.is_symlink() == (allow_softlinks and not always_copy)

        shutil.rmtree("/tmp/testenv")
