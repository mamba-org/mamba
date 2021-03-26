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

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.prefix

        # speed-up the tests
        os.environ["CONDA_PKGS_DIRS"] = TestInstall.cache

        os.makedirs(TestInstall.root_prefix, exist_ok=False)
        install("xtensor", "-n", "base")
        create("xtensor", "-n", TestInstall.env_name)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.current_prefix
        shutil.rmtree(TestInstall.root_prefix)

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        remove("xtensor", "xtl", "-n", "base")
        remove("xtensor", "xtl", "-n", TestInstall.env_name)

        res = umamba_list("xtensor", "-n", TestInstall.env_name, "--json")
        assert len(res) == 0

        os.environ["CONDA_PREFIX"] = TestInstall.prefix

    def test_empty_specs(self):
        assert "Nothing to do." in install().strip()

    @pytest.mark.parametrize("already_installed", [False, True])
    @pytest.mark.parametrize("root_prefix_env_var", [False, True])
    @pytest.mark.parametrize("target_is_root", [False, True])
    @pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
    def test_install(
        self, target_is_root, root_prefix_env_var, already_installed, env_selector
    ):
        if target_is_root:
            p = TestInstall.root_prefix
            n = "base"
            os.environ["CONDA_PREFIX"] = TestInstall.root_prefix
        else:
            p = TestInstall.prefix
            n = TestInstall.env_name

        if not root_prefix_env_var:
            os.environ.pop("MAMBA_ROOT_PREFIX")
            cmd = ("-r", TestInstall.root_prefix, "xtensor", "--json")
        else:
            cmd = "xtensor", "--json"

        if already_installed:
            install("xtensor")

        if env_selector == "prefix":
            res = install("-p", p, *cmd)
        elif env_selector == "name":
            res = install("-n", n, *cmd)
        else:
            res = install(*cmd)

        assert res["success"]
        assert not res["dry_run"]
        if already_installed:
            keys = {"dry_run", "success", "prefix", "message"}
            assert keys == set(res.keys())
        else:
            keys = {"success", "prefix", "actions", "dry_run"}
            assert keys == set(res.keys())

            action_keys = {"LINK", "PREFIX"}
            assert action_keys == set(res["actions"].keys())

            packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
            expected_packages = {"xtensor", "xtl"}
            assert expected_packages.issubset(packages)

            pkg_name = get_concrete_pkg(res, "xtensor")
            orig_file_path = get_pkg(
                pkg_name, xtensor_hpp, TestInstall.current_root_prefix
            )
            assert orig_file_path.exists()
