import json
import os
import platform
import random
import shutil
import string
import subprocess

import pytest

from .helpers import *


class TestCreate:
    @pytest.mark.parametrize(
        "root_prefix,default_root_prefix_not_mamba",
        [
            ("", False),
            ("tmproot" + random_string(), False),
            ("tmproot" + random_string(), True),
        ],
    )
    @pytest.mark.parametrize(
        "cache_prefix", ["", os.path.join(os.environ["MAMBA_ROOT_PREFIX"], "pkgs")]
    )
    def test_check_root_prefix(
        self, root_prefix, default_root_prefix_not_mamba, cache_prefix
    ):
        current_root_prefix = os.environ.pop("MAMBA_ROOT_PREFIX")

        if root_prefix:
            abs_root_prefix = os.path.expanduser(os.path.join("~", root_prefix))
            os.mkdir(abs_root_prefix)
            os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = abs_root_prefix
            if default_root_prefix_not_mamba:
                with open(os.path.join(abs_root_prefix, "some_file"), "w") as f:
                    f.write("abc")

        if cache_prefix:
            os.environ["CONDA_PKGS_DIRS"] = os.path.abspath(cache_prefix)

        try:
            if root_prefix and default_root_prefix_not_mamba:
                with pytest.raises(subprocess.CalledProcessError):
                    res = create(
                        "-n", random_string(), "xtensor", "--json", "--dry-run"
                    )
            else:
                res = create("-n", random_string(), "xtensor", "--json", "--dry-run")
        finally:  # ensure cleaning
            os.environ["MAMBA_ROOT_PREFIX"] = current_root_prefix
            if root_prefix:
                os.environ.pop("MAMBA_DEFAULT_ROOT_PREFIX")
                shutil.rmtree(abs_root_prefix)
            if cache_prefix:
                os.environ.pop("CONDA_PKGS_DIRS")
