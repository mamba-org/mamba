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
        create("-n", TestInstall.env_name, "--offline", no_dry_run=True)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.current_prefix
        shutil.rmtree(TestInstall.root_prefix)

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix

        if not Path(TestInstall.root_prefix).exists():
            os.makedirs(TestInstall.root_prefix, exist_ok=False)
        else:
            remove("-n", "base", "-a", no_dry_run=True)

        if not Path(TestInstall.prefix).exists():
            create("-n", TestInstall.env_name, "--offline", no_dry_run=True)
        else:
            remove(
                "-n", TestInstall.env_name, "-a", no_dry_run=True,
            )

        res = umamba_list("xtensor", "-n", TestInstall.env_name, "--json")
        assert len(res) == 0

        os.environ["CONDA_PREFIX"] = TestInstall.prefix

    def test_empty_specs(self):
        assert "Nothing to do." in install().strip()

    @pytest.mark.parametrize("already_installed", [False, True])
    @pytest.mark.parametrize("root_prefix_env_var", [False, True])
    @pytest.mark.parametrize(
        "target_is_root,existing_root", [(False, None), (True, False), (True, True)]
    )
    @pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
    def test_install(
        self,
        target_is_root,
        existing_root,
        root_prefix_env_var,
        already_installed,
        env_selector,
    ):
        if target_is_root:
            if not existing_root and env_selector:
                shutil.rmtree(TestInstall.root_prefix)
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
            install("xtensor", "-r", TestInstall.root_prefix, "-n", n, no_dry_run=True)

        if env_selector == "prefix":
            res = install("-p", p, *cmd)
        elif env_selector == "name":
            res = install("-n", n, *cmd)
        else:
            res = install(*cmd)

        assert res["success"]
        assert res["dry_run"] == dry_run_tests
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

            if not dry_run_tests:
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

        if specs == "CLI_only" or channels is None:
            with pytest.raises(subprocess.CalledProcessError):
                install(*cmd, default_channel=False)
        else:
            res = install(*cmd, default_channel=False)

            keys = {"success", "prefix", "actions", "dry_run"}
            assert keys.issubset(set(res.keys()))
            assert res["success"]
            assert res["dry_run"] == dry_run_tests
            assert res["prefix"] == TestInstall.prefix

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

    @pytest.mark.parametrize("valid", [False, True])
    def test_explicit_file(self, valid):
        spec_file_content = [
            "@EXPLICIT",
            "https://conda.anaconda.org/conda-forge/linux-64/xtensor-0.21.5-hc9558a2_0.tar.bz2#d330e02e5ed58330638a24601b7e4887",
        ]
        if not valid:
            spec_file_content += ["https://conda.anaconda.org/conda-forge/linux-64/xtl"]

        spec_file = os.path.join(TestInstall.root_prefix, "explicit_specs.txt")
        with open(spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = ("-p", TestInstall.prefix, "-q", "-f", spec_file)

        if valid:
            res = install(*cmd, default_channel=False)
            assert res.splitlines() == ["Linking xtensor-0.21.5-hc9558a2_0"]

            list_res = umamba_list("-p", TestInstall.prefix, "--json")
            assert len(list_res) == 1
            pkg = list_res[0]
            assert pkg["name"] == "xtensor"
            assert pkg["version"] == "0.21.5"
            assert pkg["build_string"] == "hc9558a2_0"
        else:
            with pytest.raises(subprocess.CalledProcessError):
                install(*cmd, default_channel=False)
