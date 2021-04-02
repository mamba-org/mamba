import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from .helpers import create, get_env, info, random_string


class TestInfo:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    user_config = os.path.expanduser(os.path.join("~", ".mambarc"))

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInfo.root_prefix
        os.environ["CONDA_PREFIX"] = TestInfo.prefix

        # speed-up the tests
        os.environ["CONDA_PKGS_DIRS"] = TestInfo.cache

        os.makedirs(TestInfo.root_prefix, exist_ok=False)
        create("xtensor", "-n", TestInfo.env_name, "--offline", no_dry_run=True)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInfo.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestInfo.current_prefix
        shutil.rmtree(TestInfo.root_prefix)

    @classmethod
    def teardown(cls):
        os.environ["CONDA_PREFIX"] = TestInfo.prefix

    @pytest.mark.parametrize("prefix_selection", [None, "prefix", "name"])
    def test_base(self, prefix_selection):
        os.environ["CONDA_PREFIX"] = TestInfo.root_prefix

        if prefix_selection == "prefix":
            infos = info("-p", TestInfo.root_prefix)
        elif prefix_selection == "name":
            infos = info("-n", "base")
        else:
            infos = info()

        assert f"environment : base (active)" in infos
        assert f"env location : {TestInfo.root_prefix}" in infos
        assert f"user config files : {TestInfo.user_config}" in infos
        assert f"base environment : {TestInfo.root_prefix}" in infos

    @pytest.mark.parametrize("prefix_selection", [None, "prefix", "name"])
    def test_env(self, prefix_selection):

        if prefix_selection == "prefix":
            infos = info("-p", TestInfo.prefix)
        elif prefix_selection == "name":
            infos = info("-n", TestInfo.env_name)
        else:
            infos = info()

        assert f"environment : {TestInfo.env_name} (active)" in infos
        assert f"env location : {TestInfo.prefix}" in infos
        assert f"user config files : {TestInfo.user_config}" in infos
        assert f"base environment : {TestInfo.root_prefix}" in infos

    @pytest.mark.parametrize("existing_prefix", [False, True])
    @pytest.mark.parametrize("prefix_selection", [None, "env_var", "prefix", "name"])
    def test_not_env(self, prefix_selection, existing_prefix):
        name = random_string()
        prefix = os.path.join(TestInfo.root_prefix, "envs", name)

        if existing_prefix:
            os.makedirs(prefix, exist_ok=False)

        if prefix_selection == "prefix":
            infos = info("-p", prefix)
        elif prefix_selection == "name":
            infos = info("-n", name)
        elif prefix_selection == "env_var":
            os.environ["CONDA_PREFIX"] = prefix
            infos = info()
        else:
            os.environ.pop("CONDA_PREFIX")
            infos = info()

        if prefix_selection is None:
            expected_name = "None"
            location = "-"
        elif prefix_selection == "env_var":
            expected_name = name + " (active)"
            location = prefix
        else:
            if existing_prefix:
                expected_name = name + " (not env)"
            else:
                expected_name = name + " (not found)"
            location = prefix

        assert f"environment : {expected_name}" in infos
        assert f"env location : {location}" in infos
        assert f"user config files : {TestInfo.user_config}" in infos
        assert f"base environment : {TestInfo.root_prefix}" in infos
