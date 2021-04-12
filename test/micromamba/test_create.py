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
        os.makedirs(TestCreate.root_prefix, exist_ok=True)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestCreate.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestCreate.current_prefix

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestCreate.root_prefix
        os.environ["CONDA_PREFIX"] = TestCreate.prefix

        if Path(TestCreate.root_prefix).exists():
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

    @pytest.mark.parametrize("prefix_selector", [None, "prefix", "name"])
    def test_create_empty(self, prefix_selector):

        if prefix_selector == "name":
            cmd = ("-n", TestCreate.env_name, "--json")
        elif prefix_selector == "prefix":
            cmd = ("-p", TestCreate.prefix, "--json")
        else:
            with pytest.raises(subprocess.CalledProcessError):
                create("--json")
            return

        res = create(*cmd)

        keys = {"success"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]

        assert Path(os.path.join(TestCreate.prefix, "conda-meta", "history")).exists()

    @pytest.mark.parametrize(
        "env_selector,similar_but_not_same,similar_append",
        [
            ("prefix", None, None),
            ("prefix", "root", False),
            ("prefix", "root", True),
            ("prefix", "prefix", False),
            ("prefix", "prefix", True),
            ("name", None, None),
        ],
    )
    @pytest.mark.parametrize(
        "root_exists,root_is_conda_env", [(False, False), (True, False), (True, True)]
    )
    def test_create_base(
        self,
        root_exists,
        root_is_conda_env,
        similar_but_not_same,
        similar_append,
        env_selector,
    ):
        if not root_exists:
            shutil.rmtree(TestCreate.root_prefix)
        else:
            if root_is_conda_env:
                os.makedirs(
                    os.path.join(TestCreate.root_prefix, "conda-meta"), exist_ok=False
                )

        cmd = ["xtensor"]
        if env_selector == "prefix":
            if similar_append:
                similar_prefix = os.path.join(TestCreate.root_prefix, ".")
            else:
                similar_prefix = os.path.join(
                    os.path.expanduser("~"),
                    ".",
                    os.path.relpath(TestCreate.root_prefix, os.path.expanduser("~")),
                )
            if similar_but_not_same == "root":
                cmd += [
                    "-r",
                    similar_prefix,
                    "-p",
                    TestCreate.root_prefix,
                ]
            elif similar_but_not_same == "prefix":
                cmd += [
                    "-p",
                    similar_prefix,
                    "-r",
                    TestCreate.root_prefix,
                ]
            else:
                cmd += ["-r", TestCreate.root_prefix, "-p", TestCreate.root_prefix]
        elif env_selector == "name":
            cmd += ["-n", "base"]

        with pytest.raises(subprocess.CalledProcessError):
            create(*cmd)

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

    @pytest.mark.parametrize(
        "channels", [None, "both", "CLI_only", "file_only", "file_empty"]
    )
    @pytest.mark.parametrize("env_name", [None, "both", "CLI_only", "file_only"])
    @pytest.mark.parametrize("specs", [None, "both", "CLI_only", "file_only"])
    def test_yaml_spec_file(self, channels, env_name, specs):
        spec_file_content = []
        if env_name in ("both", "file_only"):
            spec_file_content += [f"name: {TestCreate.env_name}"]
        if channels in ("both", "file_only"):
            spec_file_content += ["channels:", "  - https://repo.mamba.pm/conda-forge"]
        if specs in ("both", "file_only"):
            spec_file_content += ["dependencies:", "  - xtensor", "  - xsimd"]
        if specs == "file_empty":
            spec_file_content += ["dependencies: ~"]

        yaml_spec_file = os.path.join(TestCreate.root_prefix, "yaml_env.yml")
        with open(yaml_spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = []

        if specs not in ("file_only", None):
            cmd += ["xframe"]

        if env_name not in ("file_only", None):
            cmd += ["-n", "override_name"]

        if channels not in ("file_only", None):
            cmd += ["-c", "https://conda.anaconda.org/conda-forge"]

        cmd += ["-f", yaml_spec_file, "--json"]

        if specs is None or env_name is None or specs == "CLI_only" or channels is None:
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
            if specs not in ("file_only", None):
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

        spec_file = os.path.join(TestCreate.root_prefix, "explicit_specs.txt")
        with open(spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = ("-p", TestCreate.prefix, "-q", "-f", spec_file)

        if valid:
            res = create(*cmd, default_channel=False)
            assert res.splitlines() == ["Linking xtensor-0.21.5-hc9558a2_0"]

            list_res = umamba_list("-p", TestCreate.prefix, "--json")
            assert len(list_res) == 1
            pkg = list_res[0]
            assert pkg["name"] == "xtensor"
            assert pkg["version"] == "0.21.5"
            assert pkg["build_string"] == "hc9558a2_0"
        else:
            with pytest.raises(subprocess.CalledProcessError):
                create(*cmd, default_channel=False)
