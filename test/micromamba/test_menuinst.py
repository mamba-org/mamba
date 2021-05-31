import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

if not sys.platform.startswith("win"):
    pytest.skip("skipping windows-only tests", allow_module_level=True)

import menuinst
import win32com.client

from .helpers import create, get_env, get_umamba, random_string, remove, umamba_list


class TestMenuinst:
    root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    dirs = menuinst.win32.dirs_src

    @classmethod
    def setup_class(cls):
        pass

    @classmethod
    def teardown_class(cls):
        pass

    def test_simple_shortcut(self):
        env_name = random_string()
        # "--json"
        create("miniforge_console_shortcut", "-n", env_name, no_dry_run=True)
        prefix = os.path.join(self.root_prefix, "envs", env_name)
        d = self.dirs["user"]["start"][0]
        lnk = os.path.join(d, "Miniforge", "Miniforge Prompt (" + env_name + ").lnk")

        assert os.path.exists(lnk)

        shell = win32com.client.Dispatch("WScript.Shell")
        shortcut = shell.CreateShortCut(lnk)

        assert shortcut.TargetPath.lower() == os.getenv("COMSPEC").lower()
        assert (
            shortcut.IconLocation.lower()
            == os.path.join(prefix, "Menu", "console_shortcut.ico").lower() + ",0"
        )
        assert shortcut.Description == "Miniforge Prompt (" + env_name + ")"
        assert shortcut.Arguments == "/K " + os.path.join(
            self.root_prefix, "Scripts", "activate.bat"
        ) + " " + os.path.join(prefix)

        remove("miniforge_console_shortcut", "-n", env_name, no_dry_run=True)
        assert not os.path.exists(lnk)

    def test_shortcut_weird_env(self):
        # note Umlauts do not work yet
        os.environ["MAMBA_ROOT_PREFIX"] = str(Path("./compl i c ted").absolute())
        root_prefix = os.environ["MAMBA_ROOT_PREFIX"]

        env_name = random_string()
        # "--json"
        create("miniforge_console_shortcut", "-n", env_name, no_dry_run=True)
        prefix = os.path.join(root_prefix, "envs", env_name)
        d = self.dirs["user"]["start"][0]
        lnk = os.path.join(d, "Miniforge", "Miniforge Prompt (" + env_name + ").lnk")

        assert os.path.exists(lnk)

        shell = win32com.client.Dispatch("WScript.Shell")
        shortcut = shell.CreateShortCut(lnk)

        assert shortcut.TargetPath.lower() == os.getenv("COMSPEC").lower()
        assert (
            shortcut.IconLocation.lower()
            == os.path.join(prefix, "Menu", "console_shortcut.ico").lower() + ",0"
        )
        assert shortcut.Description == "Miniforge Prompt (" + env_name + ")"
        assert (
            shortcut.Arguments
            == '/K "'
            + os.path.join(root_prefix, "Scripts", "activate.bat")
            + '" "'
            + os.path.join(prefix)
            + '"'
        )

        remove("miniforge_console_shortcut", "-n", env_name, no_dry_run=True)
        assert not os.path.exists(lnk)

        shutil.rmtree(root_prefix)
        os.environ["MAMBA_ROOT_PREFIX"] = self.root_prefix
