import os
import platform
import shutil
from pathlib import Path

import pytest

from .helpers import *


class TestUpdate:
    def test_update(self):
        env = "random100"
        res = create("xtensor=0.18", "-n", env, "--json")
        old_pkg = get_concrete_pkg(res, "xtensor")
        pkg_info = get_concrete_pkg_info(get_env(env), old_pkg)
        old_version = pkg_info["version"]
        assert old_version.startswith("0.18")

        update_res = update("xtensor", "-n", env, "--json")
        new_pkg = get_concrete_pkg(update_res, "xtensor")
        pkg_info = get_concrete_pkg_info(get_env(env), new_pkg)
        new_version = pkg_info["version"]

        assert old_pkg != new_pkg
        assert old_version != new_version

        # This should do nothing since python is not installed!
        update_res = update("python", "-n", env, "--json")

        # TODO fix this?!
        assert update_res["message"] == "All requested packages already installed"
        assert update_res["success"] == True
        assert "action" not in update_res

        shutil.rmtree(get_env(env))
