import os
import platform
import shutil
from pathlib import Path

import pytest

from .helpers import *


class TestUpdate:
    env_name = random_string()
    root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    prefix = os.path.join(root_prefix, "envs", env_name)
    old_version = "0.18"

    @classmethod
    def setup_class(cls):
        res = create(
            f"xtensor={TestUpdate.old_version}", "-n", TestUpdate.env_name, "--json"
        )
        os.environ["CONDA_PREFIX"] = TestUpdate.prefix

        pkg = get_concrete_pkg(res, "xtensor")
        pkg_info = get_concrete_pkg_info(get_env(TestUpdate.env_name), pkg)
        version = pkg_info["version"]
        assert version.startswith(TestUpdate.old_version)

    @classmethod
    def setup(cls):
        install(
            f"xtensor={TestUpdate.old_version}", "-n", TestUpdate.env_name, "--json"
        )
        res = umamba_list("xtensor", "-n", TestUpdate.env_name, "--json")
        assert len(res) == 1
        assert res[0]["version"].startswith(TestUpdate.old_version)

    @classmethod
    def teardown_class(cls):
        os.environ["CONDA_PREFIX"] = TestUpdate.current_prefix
        shutil.rmtree(get_env(TestUpdate.env_name))

    @pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
    def test_update(self, env_selector):

        if env_selector == "prefix":
            update_res = update("xtensor", "-p", TestUpdate.prefix, "--json")
        elif env_selector == "name":
            update_res = update("xtensor", "-n", TestUpdate.env_name, "--json")
        else:
            update_res = update("xtensor", "--json")

        pkg = get_concrete_pkg(update_res, "xtensor")
        pkg_info = get_concrete_pkg_info(get_env(TestUpdate.env_name), pkg)
        version = pkg_info["version"]

        assert TestUpdate.old_version != version

        # This should do nothing since python is not installed!
        update_res = update("python", "-n", TestUpdate.env_name, "--json")

        # TODO fix this?!
        assert update_res["message"] == "All requested packages already installed"
        assert update_res["success"] == True
        assert "action" not in update_res
