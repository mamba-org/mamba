import json
import os
import platform
import random
import shutil
import string
import subprocess

import pytest

from .helpers import *


@pytest.mark.skipif(dry_run_tests == DryRun.ULTRA_DRY, reason="Running ultra dry tests")
class TestRemove:
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @staticmethod
    @pytest.fixture(scope="class")
    def root(existing_cache):
        os.environ["MAMBA_ROOT_PREFIX"] = TestRemove.root_prefix
        os.environ["CONDA_PREFIX"] = TestRemove.prefix
        create("-n", "base", no_dry_run=True)
        create("xtensor", "-n", TestRemove.env_name, no_dry_run=True)

        yield

        os.environ["MAMBA_ROOT_PREFIX"] = TestRemove.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestRemove.current_prefix
        shutil.rmtree(TestRemove.root_prefix)

    @staticmethod
    @pytest.fixture
    def env_created(root):
        if dry_run_tests == DryRun.OFF:
            install("xtensor", "-n", TestRemove.env_name)

    @pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
    def test_remove(self, env_selector, env_created):
        env_pkgs = [p["name"] for p in umamba_list("-p", TestRemove.prefix, "--json")]

        if env_selector == "prefix":
            res = remove("xtensor", "-p", TestRemove.prefix, "--json")
        elif env_selector == "name":
            res = remove("xtensor", "-n", TestRemove.env_name, "--json")
        else:
            res = remove("xtensor", "--dry-run", "--json")

        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]
        assert len(res["actions"]["UNLINK"]) == len(env_pkgs)
        for p in res["actions"]["UNLINK"]:
            assert p["name"] in env_pkgs
        assert res["actions"]["PREFIX"] == TestRemove.prefix

    def test_remove_orphaned(self, env_created):
        env_pkgs = [p["name"] for p in umamba_list("-p", TestRemove.prefix, "--json")]
        install("xframe", "-n", TestRemove.env_name, no_dry_run=True)

        res = remove("xframe", "-p", TestRemove.prefix, "--json")

        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]
        assert len(res["actions"]["UNLINK"]) == 1
        assert res["actions"]["UNLINK"][0]["name"] == "xframe"
        assert res["actions"]["PREFIX"] == TestRemove.prefix

        res = remove("xtensor", "-p", TestRemove.prefix, "--json")

        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]
        assert len(res["actions"]["UNLINK"]) == len(env_pkgs) + (
            1 if dry_run_tests == DryRun.DRY else 0
        )
        for p in res["actions"]["UNLINK"]:
            assert p["name"] in env_pkgs
        assert res["actions"]["PREFIX"] == TestRemove.prefix


class TestRemoveConfig:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @staticmethod
    @pytest.fixture(scope="class")
    def root(existing_cache):
        os.environ["MAMBA_ROOT_PREFIX"] = TestRemoveConfig.root_prefix
        os.environ["CONDA_PREFIX"] = TestRemoveConfig.prefix
        create("-n", "base", no_dry_run=True)
        create("-n", TestRemoveConfig.env_name, "--offline", no_dry_run=True)

        yield

        os.environ["MAMBA_ROOT_PREFIX"] = TestRemoveConfig.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestRemoveConfig.current_prefix
        shutil.rmtree(TestRemoveConfig.root_prefix)

    @staticmethod
    @pytest.fixture
    def env_created(root):
        os.environ["MAMBA_ROOT_PREFIX"] = TestRemoveConfig.root_prefix
        os.environ["CONDA_PREFIX"] = TestRemoveConfig.prefix

        yield

        for v in ("CONDA_CHANNELS", "MAMBA_TARGET_PREFIX"):
            if v in os.environ:
                os.environ.pop(v)

    @classmethod
    def common_tests(cls, res, root_prefix=root_prefix, target_prefix=prefix):
        assert res["root_prefix"] == root_prefix
        assert res["target_prefix"] == target_prefix
        assert res["use_target_prefix_fallback"]
        checks = (
            MAMBA_ALLOW_EXISTING_PREFIX
            | MAMBA_NOT_ALLOW_MISSING_PREFIX
            | MAMBA_NOT_ALLOW_NOT_ENV_PREFIX
            | MAMBA_EXPECT_EXISTING_PREFIX
        )
        assert res["target_prefix_checks"] == checks

    def test_specs(self, env_created):
        specs = ["xframe", "xtl"]
        cmd = list(specs)

        res = remove(*cmd, "--print-config-only")

        TestRemoveConfig.common_tests(res)
        assert res["env_name"] == ""
        assert res["specs"] == specs

    @pytest.mark.parametrize("root_prefix", (None, "env_var", "cli"))
    @pytest.mark.parametrize("target_is_root", (False, True))
    @pytest.mark.parametrize("cli_prefix", (False, True))
    @pytest.mark.parametrize("cli_env_name", (False, True))
    @pytest.mark.parametrize("env_var", (False, True))
    @pytest.mark.parametrize("fallback", (False, True))
    def test_target_prefix(
        self,
        root_prefix,
        target_is_root,
        cli_prefix,
        cli_env_name,
        env_var,
        fallback,
        env_created,
    ):
        cmd = []

        if root_prefix in (None, "cli"):
            os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = os.environ.pop(
                "MAMBA_ROOT_PREFIX"
            )

        if root_prefix == "cli":
            cmd += ["-r", TestRemoveConfig.root_prefix]

        r = TestRemoveConfig.root_prefix

        if target_is_root:
            p = r
            n = "base"
        else:
            p = TestRemoveConfig.prefix
            n = TestRemoveConfig.env_name

        if cli_prefix:
            cmd += ["-p", p]

        if cli_env_name:
            cmd += ["-n", n]

        if env_var:
            os.environ["MAMBA_TARGET_PREFIX"] = p

        if not fallback:
            os.environ.pop("CONDA_PREFIX")
        else:
            os.environ["CONDA_PREFIX"] = p

        if (cli_prefix and cli_env_name) or not (
            cli_prefix or cli_env_name or env_var or fallback
        ):
            with pytest.raises(subprocess.CalledProcessError):
                remove(*cmd, "--print-config-only")
        else:
            res = remove(*cmd, "--print-config-only")
            TestRemoveConfig.common_tests(res, root_prefix=r, target_prefix=p)
