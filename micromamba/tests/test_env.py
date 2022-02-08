import os
import platform
import shutil
from pathlib import Path

import pytest

from .helpers import *


class TestEnv:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name_1 = random_string()
    env_name_2 = random_string()
    env_name_3 = random_string()
    root_prefix = Path(os.path.join("~", "tmproot" + random_string())).expanduser()
    env_txt = Path("~/.conda/environments.txt").expanduser()
    env_txt_bkup = Path("~/.conda/environments.txt.bkup").expanduser()

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = str(cls.root_prefix)
        os.environ["CONDA_PREFIX"] = str(cls.root_prefix)

        # speed-up the tests
        os.environ["CONDA_PKGS_DIRS"] = cls.cache

        if cls.env_txt.exists():
            cls.env_txt.rename(cls.env_txt_bkup)

        res = create(
            f"",
            "-n",
            cls.env_name_1,
            "--json",
            no_dry_run=True,
        )

    @classmethod
    def setup(cls):
        pass

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = cls.current_root_prefix
        os.environ["CONDA_PREFIX"] = cls.current_prefix
        os.environ.pop("CONDA_PKGS_DIRS")
        shutil.rmtree(cls.root_prefix)

        if cls.env_txt_bkup.exists():
            cls.env_txt_bkup.rename(cls.env_txt)

    def test_env_list(self):
        env_json = umamba_env("list", "--json")

        assert "envs" in env_json
        assert len(env_json["envs"]) == 2
        assert str(self.root_prefix) == env_json["envs"][0]
        assert self.env_name_1 in env_json["envs"][1]

    def test_env_list_table(self):
        res = umamba_env("list")

        assert "Name" in res
        assert "base" in res
        assert str(self.root_prefix) in res
        lines = res.splitlines()
        for l in lines:
            if "*" in l:
                active_env_l = l
        assert str(self.root_prefix) in active_env_l

        full_env = self.root_prefix / "envs" / self.env_name_1
        os.environ["CONDA_PREFIX"] = str(full_env)

        res = umamba_env("list")

        lines = res.splitlines()
        for l in lines:
            if "*" in l:
                active_env_l = l
        assert str(full_env) in active_env_l

        os.environ["CONDA_PREFIX"] = str(self.root_prefix)

    def test_register_new_env(self):

        res = create(
            f"",
            "-n",
            self.env_name_2,
            "--json",
            no_dry_run=True,
        )
        res = create(
            f"",
            "-n",
            self.env_name_3,
            "--json",
            no_dry_run=True,
        )

        env_json = umamba_env("list", "--json")
        env_2_fp = str(self.root_prefix / "envs" / self.env_name_2)
        env_3_fp = str(self.root_prefix / "envs" / self.env_name_3)
        assert str(env_2_fp) in env_json["envs"]
        assert str(env_3_fp) in env_json["envs"]

        shutil.rmtree(env_2_fp)
        env_json = umamba_env("list", "--json")
        assert env_2_fp not in env_json["envs"]
        assert env_3_fp in env_json["envs"]
