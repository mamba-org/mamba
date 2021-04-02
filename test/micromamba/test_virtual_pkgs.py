import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import pytest

from .helpers import create, get_env, info, random_string


class TestVirtualPkgs:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    user_config = os.path.expanduser(os.path.join("~", ".mambarc"))

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestVirtualPkgs.root_prefix
        os.environ["CONDA_PREFIX"] = TestVirtualPkgs.prefix

        # speed-up the tests
        os.environ["CONDA_PKGS_DIRS"] = TestVirtualPkgs.cache

        os.makedirs(TestVirtualPkgs.root_prefix, exist_ok=False)
        create("xtensor", "-n", TestVirtualPkgs.env_name, "--offline", no_dry_run=True)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestVirtualPkgs.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestVirtualPkgs.current_prefix
        shutil.rmtree(TestVirtualPkgs.root_prefix)

    def test_virtual_packages(self):

        infos = info()

        assert "virtual packages :" in infos
        assert "__archspec=1=" in infos
        if platform.system() == "Windows":
            assert "__win" in infos
        elif platform.system() == "Darwin":
            assert "__unix=0=0" in infos
            assert "__osx" in infos
        elif platform.system() == "Linux":
            assert "__unix=0=0" in infos
            assert "__glibc" in infos
            linux_ver = platform.release().split("-", 1)[0]
            assert f"__linux={linux_ver}=0" in infos
