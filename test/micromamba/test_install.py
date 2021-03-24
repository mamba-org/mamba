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
    xsimd_hpp = "Library/include/xsimd/xsimd.hpp"
else:
    xtensor_hpp = "include/xtensor/xtensor.hpp"
    xsimd_hpp = "include/xsimd/xsimd.hpp"


class TestInstall:
    def test_empty(self):
        assert "Nothing to do." in install().strip()

    def test_already_installed(self):
        env = random_string()
        create("-n", env, "xtensor")

        res = install("xtensor", "--json", "--dry-run")
        keys = {"dry_run", "success", "prefix"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]

        shutil.rmtree(get_env(env))

    def test_not_already_installed(self):
        env = random_string()
        create("-n", env, "xtensor")
        res = install("-n", env, "bokeh", "--json", "--dry-run")
        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys == set(res.keys())
        assert res["success"]

        shutil.rmtree(get_env(env))

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
            (os.environ["MAMBA_ROOT_PREFIX"], False),
            (os.path.expanduser(os.path.join("~", "tmproot" + random_string())), False),
            (os.path.expanduser(os.path.join("~", "tmproot" + random_string())), True),
        ],
    )
    def test_target_prefix(self, root_prefix, default_root_prefix_not_mamba):
        env = "installenv" + random_string()
        current_root_prefix = os.environ.pop("MAMBA_ROOT_PREFIX")
        if default_root_prefix_not_mamba:
            os.makedirs(root_prefix, exist_ok=True)
            with open(os.path.join(root_prefix, "some_file"), "w") as f:
                f.write("abc")

        try:
            res = create("-r", root_prefix, "-n", env, "xtensor", "--json")
            pkg_name = get_concrete_pkg(res, "xtensor")
            orig_file_path = get_pkg(pkg_name, xtensor_hpp, root_prefix)
            assert orig_file_path.exists()
            assert Path(os.path.join(root_prefix, "pkgs", pkg_name)).exists()

            res = install("-r", root_prefix, "-n", env, "-y", "xsimd", "--json")
            pkg_name = get_concrete_pkg(res, "xsimd")
            orig_file_path = get_pkg(pkg_name, xsimd_hpp, root_prefix)
            assert orig_file_path.exists()
            assert Path(os.path.join(root_prefix, "pkgs", pkg_name)).exists()

        finally:  # ensure cleaning
            os.environ["MAMBA_ROOT_PREFIX"] = current_root_prefix
            if current_root_prefix != root_prefix:
                shutil.rmtree(root_prefix)

    @pytest.mark.parametrize(
        "root_prefix,default_root_prefix_not_mamba",
        [
            (os.environ["MAMBA_ROOT_PREFIX"], False),
            (os.path.expanduser(os.path.join("~", "tmproot" + random_string())), False),
            (os.path.expanduser(os.path.join("~", "tmproot" + random_string())), True),
        ],
    )
    @pytest.mark.parametrize("multiple_install", [False, True])
    def test_root_prefix(
        self, root_prefix, default_root_prefix_not_mamba, multiple_install
    ):
        current_root_prefix = os.environ.pop("MAMBA_ROOT_PREFIX")
        os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = ""
        if default_root_prefix_not_mamba:
            os.makedirs(root_prefix, exist_ok=True)
            with open(os.path.join(root_prefix, "some_file"), "w") as f:
                f.write("abc")

        try:
            try:  # Remove packages from root prefix if already installed
                remove("-r", root_prefix, "-p", root_prefix, "xsimd", "xtensor")
            except:
                pass

            create("-r", root_prefix, "-n", "otherenv", "xtensor", "--json")

            res = install("-r", root_prefix, "-n", "base", "-y", "xtensor", "--json")
            pkg_name = get_concrete_pkg(res, "xtensor")
            orig_file_path = get_pkg(pkg_name, xtensor_hpp, root_prefix)
            assert orig_file_path.exists()
            assert Path(os.path.join(root_prefix, "pkgs", pkg_name)).exists()

            if multiple_install:
                res = install("-r", root_prefix, "-n", "base", "-y", "xsimd", "--json")
                pkg_name = get_concrete_pkg(res, "xsimd")
                orig_file_path = get_pkg(pkg_name, xsimd_hpp, root_prefix)
                assert orig_file_path.exists()
                assert Path(os.path.join(root_prefix, "pkgs", pkg_name)).exists()

        finally:  # ensure cleaning
            os.environ["MAMBA_ROOT_PREFIX"] = current_root_prefix
            if current_root_prefix != root_prefix:
                shutil.rmtree(root_prefix)
