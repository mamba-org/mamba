import json
import os
import platform
import shutil
import subprocess

import pytest

from .helpers import create, get_env, random_string, shell


class TestShell:
    @pytest.mark.parametrize(
        "shell_type", ["bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh"]
    )
    def test_hook(self, shell_type):
        assert shell("hook", "-s", shell_type)

    @pytest.mark.parametrize("shell_type", ["bash", "posix", "powershell", "cmd.exe"])
    @pytest.mark.parametrize("env_name", ["base", "activate_env"])
    @pytest.mark.parametrize("env_exists", [False, True])
    def test_activate(self, shell_type, env_name, env_exists):
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

        root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
        if env_name != "base" and env_exists:
            create("xtensor", "-n", env_name)

        assert shell("activate", "-s", shell_type, "-p", env_name)

        if env_name != "base" and env_exists:
            shutil.rmtree(get_env(env_name))

    @pytest.mark.parametrize("shell_type", ["bash", "powershell", "cmd.exe"])
    @pytest.mark.parametrize(
        "prefix",
        [
            "",
            os.environ["MAMBA_ROOT_PREFIX"],
            os.path.expanduser(os.path.join("~", "tmproot" + random_string())),
        ],
    )
    def test_init(self, shell_type, prefix):
        if (
            (platform.system() == "Linux" and shell_type != "bash")
            or (
                platform.system() == "Windows"
                and shell_type not in ("cmd.exe", "powershell")
            )
            or (platform.system() == "Darwin" and shell_type not in ("zsh", "bash"))
        ):
            pytest.skip("Incompatible shell/OS")

        current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]

        if prefix:
            assert shell("-y", "init", "-s", shell_type, "-p", prefix)
        else:
            assert shell("-y", "init", "-s", shell_type)

        if prefix and prefix != current_root_prefix:
            shutil.rmtree(prefix)

        # clean-up
        shell("init", "-y", "-s", shell_type, "-p", current_root_prefix)
