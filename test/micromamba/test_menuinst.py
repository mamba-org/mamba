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
        print(d)
        print(self.dirs)

        lnk = os.path.join(d, "Miniforge", "Miniforge Prompt (" + env_name + ").lnk")
        d2 = self.dirs["system"]["start"][0]
        lnk2 = os.path.join(d2, "Miniforge", "Miniforge Prompt (" + env_name + ").lnk")

        assert (os.path.exists(lnk) or os.path.exists(lnk2))

        if os.path.exists(lnk2):
            lnk = lnk2

        shell = win32com.client.Dispatch("WScript.Shell")
        shortcut = shell.CreateShortCut(lnk)

        assert shortcut.TargetPath.lower() == os.getenv("COMSPEC").lower()
        icon_location = shortcut.IconLocation
        icon_location_path, icon_location_index = icon_location.split(",")
        assert Path(icon_location_path) == (
            Path(prefix) / "Menu" / "console_shortcut.ico"
        )
        assert (icon_location_index, "0")

        assert shortcut.Description == "Miniforge Prompt (" + env_name + ")"
        assert shortcut.Arguments == "/K " + str(
            Path(self.root_prefix, "Scripts", "activate.bat")
        ) + " " + str(Path(prefix))

        remove("miniforge_console_shortcut", "-n", env_name, no_dry_run=True)
        assert not os.path.exists(lnk)

    def test_shortcut_weird_env(self):
        print(os.environ)
        # note Umlauts do not work yet
        os.environ["MAMBA_ROOT_PREFIX"] = str(Path("./compl i c ted").absolute())
        root_prefix = os.environ["MAMBA_ROOT_PREFIX"]

        env_name = random_string()
        # "--json"
        create("miniforge_console_shortcut", "-n", env_name, no_dry_run=True)
        prefix = os.path.join(root_prefix, "envs", env_name)
        d = self.dirs["user"]["start"][0]
        lnk = os.path.join(d, "Miniforge", "Miniforge Prompt (" + env_name + ").lnk")

        lnk = os.path.join(d, "Miniforge", "Miniforge Prompt (" + env_name + ").lnk")
        d2 = self.dirs["system"]["start"][0]
        lnk2 = os.path.join(d2, "Miniforge", "Miniforge Prompt (" + env_name + ").lnk")

        assert (os.path.exists(lnk) or os.path.exists(lnk2))

        if os.path.exists(lnk2):
            lnk = lnk2

        shell = win32com.client.Dispatch("WScript.Shell")
        shortcut = shell.CreateShortCut(lnk)

        assert shortcut.TargetPath.lower() == os.getenv("COMSPEC").lower()

        icon_location = shortcut.IconLocation
        icon_location_path, icon_location_index = icon_location.split(",")
        assert Path(icon_location_path) == (
            Path(prefix) / "Menu" / "console_shortcut.ico"
        )
        assert (icon_location_index, "0")

        assert shortcut.Description == "Miniforge Prompt (" + env_name + ")"
        assert (
            shortcut.Arguments
            == '/K "'
            + str(Path(root_prefix) / "Scripts" / "activate.bat")
            + '" "'
            + str(Path(prefix))
            + '"'
        )

        remove("miniforge_console_shortcut", "-n", env_name, no_dry_run=True)
        assert not os.path.exists(lnk)

        shutil.rmtree(root_prefix)
        os.environ["MAMBA_ROOT_PREFIX"] = self.root_prefix
