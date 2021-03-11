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

    @pytest.mark.parametrize(
        "root_prefix,default_root_prefix_not_mamba",
        [
            ("", False),
            ("tmproot" + random_string(), False),
            ("tmproot" + random_string(), True),
        ],
    )
    @pytest.mark.parametrize(
        "cache_prefix", ["", os.path.join(os.environ["MAMBA_ROOT_PREFIX"] + "pkgs")]
    )
    def test_check_root_prefix(
        self, root_prefix, default_root_prefix_not_mamba, cache_prefix
    ):
        current_root_prefix = os.environ.pop("MAMBA_ROOT_PREFIX")

        if root_prefix:
            abs_root_prefix = os.path.expanduser(os.path.join("~", root_prefix))
            os.mkdir(abs_root_prefix)
            os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = abs_root_prefix
            if default_root_prefix_not_mamba:
                with open(os.path.join(abs_root_prefix, "some_file"), "w") as f:
                    f.write("abc")

        if cache_prefix:
            os.environ["CONDA_PKGS_DIRS"] = os.path.abspath(cache_prefix)

        try:
            if (not cache_prefix) and root_prefix and default_root_prefix_not_mamba:
                with pytest.raises(subprocess.CalledProcessError):
                    res = create(
                        "-n", random_string(), "xtensor", "--json", "--dry-run"
                    )
            else:
                res = create("-n", random_string(), "xtensor", "--json", "--dry-run")
        finally:  # ensure cleaning
            os.environ["MAMBA_ROOT_PREFIX"] = current_root_prefix
            if root_prefix:
                os.environ.pop("MAMBA_DEFAULT_ROOT_PREFIX")
                shutil.rmtree(abs_root_prefix)
            if cache_prefix:
                os.environ.pop("CONDA_PKGS_DIRS")


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

        xf = Path(env) / xtensor_hpp
        assert xf.exists()
        stat_xf = xf.stat()

        pkg_name = get_concrete_pkg(res, "xtensor")
        orig_file_path = get_pkg(pkg_name, xtensor_hpp)

        stat_orig = orig_file_path.stat()
        assert stat_orig.st_dev != stat_xf.st_dev and stat_orig.st_ino != stat_xf.st_ino
        assert xf.is_symlink() == (allow_softlinks and not always_copy)

        shutil.rmtree("/tmp/testenv")


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
