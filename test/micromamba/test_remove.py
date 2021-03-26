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
    @pytest.mark.parametrize("use_prefix", [False, True])
    @pytest.mark.parametrize("env_name", ["", random_string()])
    def test_remove(self, env_name, use_prefix):
        if env_name:
            env = env_name
        else:
            env = random_string()

        root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
        current_prefix = os.environ["CONDA_PREFIX"]
        prefix = os.path.join(root_prefix, "envs", env)
        os.environ["CONDA_PREFIX"] = prefix

        try:
            if env_name:
                if use_prefix:
                    create("-n", env, "xtensor")
                    res = remove("xtensor", "-p", prefix, "--dry-run", "--json")
                else:
                    create("-n", env, "xtensor")
                    res = remove("xtensor", "-n", env, "--dry-run", "--json")
            else:
                create("-n", env, "xtensor")
                res = remove("xtensor", "--dry-run", "--json")

            keys = {"dry_run", "success", "prefix", "actions"}
            assert keys.issubset(set(res.keys()))
            assert res["success"]
            assert len(res["actions"]["UNLINK"]) == 1
            assert res["actions"]["UNLINK"][0]["name"] == "xtensor"
            assert res["actions"]["PREFIX"] == prefix
        finally:
            os.environ["CONDA_PREFIX"] = current_prefix
            shutil.rmtree(get_env(env))
