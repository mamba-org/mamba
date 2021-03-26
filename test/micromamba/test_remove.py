import json
import os
import platform
import random
import shutil
import string
import subprocess

import pytest

from .helpers import *


class TestRemove:
    env_name = random_string()
    root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    prefix = Path(os.path.join(root_prefix, "envs", env_name)).__str__()

    @classmethod
    def setup_class(cls):
        create("xtensor", "-n", TestRemove.env_name)
        os.environ["CONDA_PREFIX"] = TestRemove.prefix

    @classmethod
    def setup(cls):
        install("xtensor", "-n", TestRemove.env_name)
        res = umamba_list("xtensor", "-n", TestRemove.env_name, "--json")
        assert len(res) == 1

    @classmethod
    def teardown_class(cls):
        os.environ["CONDA_PREFIX"] = TestRemove.current_prefix
        shutil.rmtree(get_env(TestRemove.env_name))

    @pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
    def test_remove(self, env_selector):
        if env_selector == "prefix":
            res = remove("xtensor", "-p", TestRemove.prefix, "--dry-run", "--json")
        elif env_selector == "name":
            res = remove("xtensor", "-n", TestRemove.env_name, "--dry-run", "--json")
        else:
            res = remove("xtensor", "--dry-run", "--json")

        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys.issubset(set(res.keys()))
        assert res["success"]
        assert len(res["actions"]["UNLINK"]) == 1
        assert res["actions"]["UNLINK"][0]["name"] == "xtensor"
        assert res["actions"]["PREFIX"] == TestRemove.prefix
