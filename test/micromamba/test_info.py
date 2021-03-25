import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from .helpers import create, get_env, info


class TestInfo:
    @pytest.mark.parametrize(
        "prefix_opt,prefix", [("-p", os.environ["MAMBA_ROOT_PREFIX"]), ("-n", "base")]
    )
    def test_base(self, prefix_opt, prefix):
        root_prefix = Path(os.environ["MAMBA_ROOT_PREFIX"]).__str__()
        location = root_prefix
        user_config = os.path.expanduser(os.path.join("~", ".mambarc"))

        infos = info(prefix_opt, prefix)
        assert f"active environment : base" in infos
        assert f"active env location : {location}" in infos
        assert f"user config files : {user_config}" in infos
        assert f"base environment : {root_prefix}" in infos

    @pytest.mark.parametrize(
        "prefix_opt,prefix",
        [
            ("-p", os.path.join(os.environ["MAMBA_ROOT_PREFIX"], "envs", "infoenv")),
            ("-n", "infoenv"),
        ],
    )
    def test_env(self, prefix_opt, prefix):
        env = "infoenv"
        res = create("xtensor", "-n", env, "--json")

        root_prefix = Path(os.environ["MAMBA_ROOT_PREFIX"]).__str__()
        location = os.path.join(root_prefix, "envs", env)
        user_config = os.path.expanduser(os.path.join("~", ".mambarc"))

        infos = info(prefix_opt, prefix)
        assert f"active environment : {env}" in infos
        assert f"active env location : {location}" in infos
        assert f"user config files : {user_config}" in infos
        assert f"base environment : {root_prefix}" in infos

        shutil.rmtree(get_env(env))
