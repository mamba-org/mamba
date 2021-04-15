import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import pytest

from .helpers import create, get_env, get_umamba, info, random_string, shell


class TestShell:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.root_prefix

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.current_root_prefix

    @classmethod
    def setup(cls):
        os.makedirs(TestShell.root_prefix, exist_ok=False)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.current_root_prefix

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.root_prefix
        shutil.rmtree(TestShell.root_prefix)

    @pytest.mark.parametrize(
        "shell_type", ["bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh"]
    )
    def test_hook(self, shell_type):
        res = shell("hook", "-s", shell_type)

        mamba_exe = get_umamba()
        # suspend long path support on Windows
        # if platform.system() == "Windows":
        # mamba_exe = f"\\\\?\\{mamba_exe}"

        if shell_type == "powershell":
            umamba = get_umamba()
            assert f"$Env:MAMBA_EXE='{mamba_exe}'" in res
        elif shell_type in ("zsh", "bash", "posix"):
            assert res.count(mamba_exe) == 5
        elif shell_type == "xonsh":
            assert res.count(mamba_exe) == 8
        elif shell_type == "cmd.exe":
            assert res

    @pytest.mark.parametrize("shell_type", ["bash", "posix", "powershell", "cmd.exe"])
    @pytest.mark.parametrize("root", [False, True])
    @pytest.mark.parametrize("env_exists", [False, True])
    @pytest.mark.parametrize("expanded_home", [False, True])
    @pytest.mark.parametrize("prefix_type", ["prefix", "name"])
    def test_activate(self, shell_type, root, env_exists, prefix_type, expanded_home):
        if (
            (platform.system() == "Linux" and shell_type not in ("bash", "posix"))
            or (
                platform.system() == "Windows"
                and shell_type not in ("cmd.exe", "powershell")
            )
            or (
                platform.system() == "Darwin"
                and shell_type not in ("zsh", "bash", "posix")
            )
        ):
            pytest.skip("Incompatible shell/OS")

        if not root and env_exists:
            create("-n", TestShell.env_name, "-q", "--offline", no_dry_run=True)

        if prefix_type == "prefix":
            if expanded_home:
                cmd = ("activate", "-s", shell_type, "-p", TestShell.prefix)
            else:
                cmd = (
                    "activate",
                    "-s",
                    shell_type,
                    "-p",
                    TestShell.prefix.replace(os.path.expanduser("~"), "~"),
                )
        else:
            cmd = ("activate", "-s", shell_type, "-p", TestShell.env_name)

        res = shell(*cmd)

        # TODO: improve this test
        assert res

        if shell_type == "bash":
            assert f"export CONDA_PREFIX='{TestShell.prefix}'" in res
            assert f"export CONDA_DEFAULT_ENV='{TestShell.env_name}'" in res
            assert f"export CONDA_PROMPT_MODIFIER='({TestShell.env_name}) '" in res

    @pytest.mark.parametrize("shell_type", ["bash", "powershell", "cmd.exe"])
    @pytest.mark.parametrize("prefix_selector", [None, "prefix"])
    def test_init(self, shell_type, prefix_selector):
        if (
            (platform.system() == "Linux" and shell_type != "bash")
            or (
                platform.system() == "Windows"
                and shell_type not in ("cmd.exe", "powershell")
            )
            or (platform.system() == "Darwin" and shell_type not in ("zsh", "bash"))
        ):
            pytest.skip("Incompatible shell/OS")

        if prefix_selector:
            shell("-y", "init", "-s", shell_type, "-p", TestShell.root_prefix)
        else:
            cwd = os.getcwd()
            os.chdir(TestShell.root_prefix)
            shell("-y", "init", "-s", shell_type, cwd=cwd)
            os.chdir(cwd)

        assert (
            Path(os.path.join(TestShell.root_prefix, "condabin")).is_dir()
            or Path(os.path.join(TestShell.root_prefix, "etc", "profile.d")).is_dir()
        )

        shell("init", "-y", "-s", shell_type, "-p", TestShell.current_root_prefix)
