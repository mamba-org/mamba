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


class TestCreate:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    other_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestCreate.root_prefix
        os.environ["CONDA_PREFIX"] = TestCreate.prefix

        # speed-up the tests
        os.environ["CONDA_PKGS_DIRS"] = TestCreate.cache

    @classmethod
    def setup(cls):
        os.makedirs(TestCreate.root_prefix, exist_ok=False)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestCreate.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestCreate.current_prefix

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestCreate.root_prefix
        os.environ["CONDA_PREFIX"] = TestCreate.prefix
        shutil.rmtree(TestCreate.root_prefix)
        if Path(TestCreate.other_prefix).exists():
            shutil.rmtree(TestCreate.other_prefix)

    @pytest.mark.parametrize("root_prefix_env_var", [False, True])
    @pytest.mark.parametrize(
        "env_selector,outside_root_prefix",
        [("prefix", False), ("prefix", True), ("name", False)],
    )
    def test_create(self, root_prefix_env_var, env_selector, outside_root_prefix):
        if not root_prefix_env_var:
            os.environ.pop("MAMBA_ROOT_PREFIX")
            cmd = ("-r", TestCreate.root_prefix, "xtensor", "--json")
        else:
            cmd = ("xtensor", "--json")

        if outside_root_prefix:
            p = TestCreate.other_prefix
        else:
            p = TestCreate.prefix

        if env_selector == "prefix":
            res = create("-p", p, *cmd)
        elif env_selector == "name":
            res = create("-n", TestCreate.env_name, *cmd)

        assert res["success"]
        assert res["dry_run"] == dry_run_tests

        keys = {"success", "prefix", "actions", "dry_run"}
        assert keys.issubset(set(res.keys()))

        action_keys = {"LINK", "PREFIX"}
        assert action_keys.issubset(set(res["actions"].keys()))

        packages = {pkg["name"] for pkg in res["actions"]["LINK"]}
        expected_packages = {"xtensor", "xtl"}
        assert expected_packages.issubset(packages)

        if not dry_run_tests:
            pkg_name = get_concrete_pkg(res, "xtensor")
            orig_file_path = get_pkg(
                pkg_name, xtensor_hpp, TestCreate.current_root_prefix
            )
            assert orig_file_path.exists()

    @pytest.mark.parametrize("env_selector", ["prefix", "name"])
    @pytest.mark.parametrize("root_non_empty", [False, True])
    def test_create_base(self, root_non_empty, env_selector):

        with open(os.path.join(TestCreate.root_prefix, "some_file"), "w") as f:
            f.write("abc")

        with pytest.raises(subprocess.CalledProcessError):
            if env_selector == "prefix":
                create("-p", TestCreate.root_prefix, "xtensor")
            elif env_selector == "name":
                create("-n", "base", "xtensor")

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
            res = create(
                "-n",
                TestCreate.env_name,
                "xtensor",
                "--json",
                "--dry-run",
                "--channel-alias",
                alias,
            )
            ca = alias.rstrip("/")
        else:
            res = create("-n", TestCreate.env_name, "xtensor", "--json", "--dry-run")
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
            spec_file_content += [f"name: {TestCreate.env_name}"]
        if channels not in ("CLI_only", None):
            spec_file_content += ["channels:", "  - https://repo.mamba.pm/conda-forge"]
        if specs != "CLI_only":
            spec_file_content += ["dependencies:", "  - xtensor", "  - xsimd"]

        yaml_spec_file = os.path.join(TestCreate.root_prefix, "yaml_env.yml")
        with open(yaml_spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = []

        if specs != "file_only":
            cmd += ["xframe"]

        if env_name not in ("file_only", None):
            cmd += ["-n", "override_name"]

        if channels not in ("file_only", None):
            cmd += ["-c", "https://conda.anaconda.org/conda-forge"]

        cmd += ["-f", yaml_spec_file, "--json"]

        if env_name is None or specs == "CLI_only" or channels is None:
            with pytest.raises(subprocess.CalledProcessError):
                create(*cmd, default_channel=False)
        else:
            res = create(*cmd, default_channel=False)

            keys = {"success", "prefix", "actions", "dry_run"}
            assert keys.issubset(set(res.keys()))
            assert res["success"]
            assert res["dry_run"] == dry_run_tests
            if env_name == "file_only":
                assert res["prefix"] == TestCreate.prefix
            else:
                assert res["prefix"] == os.path.join(
                    TestCreate.root_prefix, "envs", "override_name"
                )

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
