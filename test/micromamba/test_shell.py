import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import pytest

from .helpers import create, get_env, info, random_string, shell


class TestShell:
    @pytest.mark.parametrize(
        "shell_type", ["bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh"]
    )
    def test_hook(self, shell_type):
        root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
        clean_root = not Path(root_prefix).exists()

        assert shell("hook", "-s", shell_type)

        if Path(root_prefix).exists() and clean_root:
            shutil.rmtree(root_prefix)

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
        clean_root = not Path(root_prefix).exists()

        if env_name != "base" and env_exists:
            create("xtensor", "-n", env_name)

        # TODO: improve this test
        assert shell("activate", "-s", shell_type, "-p", env_name)

        if env_name != "base" and env_exists:
            shutil.rmtree(get_env(env_name))
        if Path(root_prefix).exists() and clean_root:
            shutil.rmtree(root_prefix)

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
        clean_root = not Path(current_root_prefix).exists()

        if prefix:
            shell("-y", "init", "-s", shell_type, "-p", prefix)
        else:
            shell("-y", "init", "-s", shell_type)

        assert Path(prefix).exists()
        assert Path(prefix).is_dir()
        assert (
            Path(os.path.join(prefix, "condabin")).is_dir()
            or Path(os.path.join(prefix, "etc", "profile.d")).is_dir()
        )

        if prefix and prefix != current_root_prefix:
            shutil.rmtree(prefix)

        # clean-up
        if clean_root:
            if Path(current_root_prefix).exists():
                shutil.rmtree(current_root_prefix)
        else:
            shell("init", "-y", "-s", shell_type, "-p", current_root_prefix)
