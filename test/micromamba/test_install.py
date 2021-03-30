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
        remove("xtensor", "xtl", "-n", "base")
        remove("xtensor", "xtl", "-n", TestInstall.env_name)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.current_prefix
        shutil.rmtree(TestInstall.root_prefix)

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        remove("xtensor", "xtl", "xsimd", "xframe", "-n", "base")
        remove("xtensor", "xtl", "xsimd", "xframe", "-n", TestInstall.env_name)

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
            assert keys.issubset(set(res.keys()))
        else:
            keys = {"success", "prefix", "actions", "dry_run"}
            assert keys.issubset(set(res.keys()))

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

    @pytest.mark.parametrize(
        "alias",
        [
            "",
            "https://conda.anaconda.org/",
            "https://repo.mamba.pm/",
            "https://repo.mamba.pm",
        ],
    )
    def test_channel_alias(self, alias):
        if alias:
            res = install("xtensor", "--json", "--dry-run", "--channel-alias", alias)
            ca = alias.rstrip("/")
        else:
            res = install("xtensor", "--json", "--dry-run")
            ca = "https://conda.anaconda.org"

        for l in res["actions"]["LINK"]:
            assert l["channel"].startswith(f"{ca}/conda-forge/")
            assert l["url"].startswith(f"{ca}/conda-forge/")

    @pytest.mark.parametrize("channels", [None, "both", "CLI_only", "file_only"])
    @pytest.mark.parametrize("env_name", [None, "both", "CLI_only", "file_only"])
    @pytest.mark.parametrize("specs", ["both", "CLI_only", "file_only"])
    def test_yaml_spec_file(self, channels, env_name, specs):
        spec_file_content = []
        if env_name not in ("CLI_only", None):
            spec_file_content += [f"name: {TestInstall.env_name}"]
        if channels not in ("CLI_only", None):
            spec_file_content += ["channels:", "  - https://repo.mamba.pm/conda-forge"]
        if specs != "CLI_only":
            spec_file_content += ["dependencies:", "  - xtensor", "  - xsimd"]

        yaml_spec_file = os.path.join(TestInstall.prefix, "yaml_env.yml")
        with open(yaml_spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = []

        if specs != "file_only":
            cmd += ["xframe"]

        if env_name not in ("file_only", None):
            cmd += [
                "-n",
                TestInstall.env_name,
            ]

        if channels not in ("file_only", None):
            cmd += ["-c", "https://conda.anaconda.org/conda-forge"]

        cmd += ["-f", yaml_spec_file, "--json"]

        if env_name == "both" or specs == "CLI_only" or channels is None:
            with pytest.raises(subprocess.CalledProcessError):
                install(*cmd, default_channel=False)
        else:
            res = install(*cmd, default_channel=False)

            assert res["success"]
            assert not res["dry_run"]

            keys = {"success", "prefix", "actions", "dry_run"}
            assert keys.issubset(set(res.keys()))

            action_keys = {"LINK", "PREFIX"}
            assert action_keys.issubset(set(res["actions"].keys()))

            packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
            expected_packages = ["xtensor", "xtl", "xsimd"]
            if specs != "file_only":
                expected_packages.append("xframe")
            assert set(expected_packages).issubset(packages)

            if channels == "file_only":
                expected_channel = "https://repo.mamba.pm/conda-forge"
            else:
                expected_channel = "https://conda.anaconda.org/conda-forge"

            for l in res["actions"]["LINK"]:
                assert l["channel"].startswith(expected_channel)
                assert l["url"].startswith(expected_channel)
