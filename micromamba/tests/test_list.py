import json
import os
import re
import shutil
import subprocess

import pytest

from .helpers import create, get_env, get_umamba, random_string, umamba_list


class TestList:
    env_name = random_string()
    root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        create("xtensor=0.24.5", "-n", TestList.env_name, "--json", no_dry_run=True)
        os.environ["CONDA_PREFIX"] = TestList.prefix

    @classmethod
    def teardown_class(cls):
        os.environ["CONDA_PREFIX"] = TestList.current_prefix
        shutil.rmtree(get_env(TestList.env_name))

    @pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
    @pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
    def test_list(self, env_selector, quiet_flag):
        if env_selector == "prefix":
            res = umamba_list("-p", TestList.prefix, "--json", quiet_flag)
        elif env_selector == "name":
            res = umamba_list("-n", TestList.env_name, "--json", quiet_flag)
        else:
            res = umamba_list("--json", quiet_flag)

        assert len(res) > 2

        names = [i["name"] for i in res]
        assert "xtensor" in names
        assert "xtl" in names

    @pytest.mark.parametrize("env_selector", ["name", "prefix"])
    def test_not_existing(self, env_selector):
        if env_selector == "prefix":
            cmd = (
                "-p",
                os.path.join(TestList.root_prefix, "envs", random_string()),
                "--json",
            )
        elif env_selector == "name":
            cmd = ("-n", random_string(), "--json")

        with pytest.raises(subprocess.CalledProcessError):
            umamba_list(*cmd)

    def test_not_environment(self):
        with pytest.raises(subprocess.CalledProcessError):
            umamba_list(
                "-p",
                os.path.join(TestList.root_prefix, "envs"),
                "--json",
            )

    @pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
    def test_regex(self, quiet_flag):
        full_res = umamba_list("--json")
        names = sorted([i["name"] for i in full_res])

        filtered_res = umamba_list("\\**", "--json", quiet_flag)
        filtered_names = sorted([i["name"] for i in filtered_res])
        assert filtered_names == names

        filtered_res = umamba_list("^xt", "--json", quiet_flag)
        filtered_names = sorted([i["name"] for i in filtered_res])
        assert filtered_names == ["xtensor", "xtl"]
