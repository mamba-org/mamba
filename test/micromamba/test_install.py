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
        res = create(*create_args)

        xf = Path(env + "/include/xtensor/xtensor.hpp")
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, "include/xtensor/xtensor.hpp")

        stat_orig = orig_file_path.stat()
        assert stat_orig.st_dev != stat_xf.st_dev and stat_orig.st_ino != stat_xf.st_ino
        assert xf.is_symlink() == (allow_softlinks and not always_copy)

        shutil.rmtree("/tmp/testenv")


class TestPkgCache:
    def test_extracted_file_deleted(self):
        ref_env = "x"
        res = create("xtensor", "-n", ref_env, "--json")
        xf = get_env(ref_env, "include/xtensor/xtensor.hpp")
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, "include/xtensor/xtensor.hpp")
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        os.remove(orig_file_path)

        env = "x1"
        res = create("xtensor", "-n", env, "--json")
        xf = get_env(env, "include/xtensor/xtensor.hpp")
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
        xf = get_env(ref_env, "include/xtensor/xtensor.hpp")
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, "include/xtensor/xtensor.hpp")
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        with open(orig_file_path, "w") as f:
            f.write("//corruption")

        env = "x1"
        res = create("xtensor", "-n", env, "--json", "--safety-checks", safety_checks)
        xf = get_env(env, "include/xtensor/xtensor.hpp")
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
        xf = get_env(ref_env, "include/xtensor/xtensor.hpp")
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, "include/xtensor/xtensor.hpp")
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        os.remove(get_tarball(pkg_name))

        env = "x1"
        res = create("xtensor", "-n", env, "--json")
        xf = get_env(env, "include/xtensor/xtensor.hpp")
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
        xf = get_env(ref_env, "include/xtensor/xtensor.hpp")
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, "include/xtensor/xtensor.hpp")
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        pkg_size = get_tarball(pkg_name).stat().st_size
        os.remove(orig_file_path)
        os.remove(get_tarball(pkg_name))

        env = "x1"
        res = create("xtensor", "-n", env, "--json")
        xf = get_env(env, "include/xtensor/xtensor.hpp")
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
        xf = get_env(ref_env, "include/xtensor/xtensor.hpp")
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, "include/xtensor/xtensor.hpp")
        stat_orig = orig_file_path.stat()

        assert stat_orig.st_dev == stat_xf.st_dev and stat_orig.st_ino == stat_xf.st_ino

        pkg_size = get_tarball(pkg_name).stat().st_size
        os.remove(orig_file_path)
        os.remove(get_tarball(pkg_name))
        with open(get_tarball(pkg_name), "w") as f:
            f.write("")

        env = "x1"
        res = create("xtensor", "-n", env, "--json")
        xf = get_env(env, "include/xtensor/xtensor.hpp")
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
        xf = get_env(ref_env, "include/xtensor/xtensor.hpp")
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, "include/xtensor/xtensor.hpp")
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
