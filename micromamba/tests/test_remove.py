import json
import os
import platform
import random
import shutil
import string
import subprocess
import time
from pathlib import Path

import pytest

from .helpers import *

__this_dir__ = Path(__file__).parent.resolve()


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

    def test_remove_force(self, env_created):
        # check that we can remove a package without solving the environment (putting
        # it in a bad state, actually)
        env_pkgs = [p["name"] for p in umamba_list("-p", TestRemove.prefix, "--json")]
        install("xframe", "-n", TestRemove.env_name, no_dry_run=True)

        res = remove("xtl", "-p", TestRemove.prefix, "--json", "--force")

        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]
        assert len(res["actions"]["UNLINK"]) == 1
        assert res["actions"]["UNLINK"][0]["name"] == "xtl"
        assert res["actions"]["PREFIX"] == TestRemove.prefix

    def test_remove_no_prune_deps(self, env_created):
        env_pkgs = [p["name"] for p in umamba_list("-p", TestRemove.prefix, "--json")]
        install("xframe", "-n", TestRemove.env_name, no_dry_run=True)

        res = remove("xtensor", "-p", TestRemove.prefix, "--json", "--no-prune-deps")

        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]
        assert len(res["actions"]["UNLINK"]) == 2
        removed_names = [x["name"] for x in res["actions"]["UNLINK"]]
        assert "xtensor" in removed_names
        assert "xframe" in removed_names
        assert res["actions"]["PREFIX"] == TestRemove.prefix

    def test_remove_in_use(self, env_created):
        install("python=3.9", "-n", self.env_name, "--json", no_dry_run=True)
        if platform.system() == "Windows":
            pyexe = Path(self.prefix) / "python.exe"
        else:
            pyexe = Path(self.prefix) / "bin" / "python"

        env = get_fake_activate(self.prefix)

        pyproc = subprocess.Popen(
            pyexe, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env
        )
        time.sleep(1)

        res = remove("python", "-v", "-p", self.prefix, no_dry_run=True)

        if platform.system() == "Windows":
            pyexe_trash = Path(str(pyexe) + ".mamba_trash")
            assert pyexe.exists() == False
            pyexe_trash_exists = pyexe_trash.exists()
            trash_file = Path(self.prefix) / "conda-meta" / "mamba_trash.txt"

            if pyexe_trash_exists:
                assert pyexe_trash.exists()
                assert trash_file.exists()
                all_trash_files = list(Path(self.prefix).rglob("*.mamba_trash"))

                with open(trash_file, "r") as fi:
                    lines = [x.strip() for x in fi.readlines()]
                    assert all([l.endswith(".mamba_trash") for l in lines])
                    assert len(all_trash_files) == len(lines)
                    linesp = [Path(self.prefix) / l for l in lines]
                    for atf in all_trash_files:
                        assert atf in linesp
            else:
                assert trash_file.exists() == False
                assert pyexe_trash.exists() == False
            # No change if file still in use
            install("cpp-filesystem", "-n", self.env_name, "--json", no_dry_run=True)

            if pyexe_trash_exists:
                assert trash_file.exists()
                assert pyexe_trash.exists()

                with open(trash_file, "r") as fi:
                    lines = [x.strip() for x in fi.readlines()]
                    assert all([l.endswith(".mamba_trash") for l in lines])
                    assert len(all_trash_files) == len(lines)
                    linesp = [Path(self.prefix) / l for l in lines]
                    for atf in all_trash_files:
                        assert atf in linesp
            else:
                assert trash_file.exists() == False
                assert pyexe_trash.exists() == False

            subprocess.Popen("TASKKILL /F /PID {pid} /T".format(pid=pyproc.pid))
            # check that another env mod clears lingering trash files
            time.sleep(0.5)
            install("xsimd", "-n", self.env_name, "--json", no_dry_run=True)
            assert trash_file.exists() == False
            assert pyexe_trash.exists() == False

        else:
            assert pyexe.exists() == False
            pyproc.kill()


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

    def test_remove_then_clean(self, env_created):
        env_file = __this_dir__ / "env-requires-pip-install.yaml"
        env_name = "env_to_clean"
        create("-n", env_name, "-f", env_file, no_dry_run=True)
        remove("-n", env_name, "pip", no_dry_run=True)
        clean("-ay", no_dry_run=True)

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
