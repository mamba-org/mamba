import os
import platform
import shutil
from pathlib import Path

import pytest

from .helpers import *


class TestUpdate:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    old_version = "0.18.3"

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdate.root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdate.prefix

        res = create(
            f"xtensor={TestUpdate.old_version}", "-n", TestUpdate.env_name, "--json"
        )

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
        os.environ["MAMBA_ROOT_PREFIX"] = TestUpdate.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestUpdate.current_prefix
        shutil.rmtree(TestUpdate.root_prefix)

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

    @pytest.mark.parametrize(
        "alias",
        [
            "",
            "https://conda.anaconda.org/",
            "https://repo.mamba.pm/",
            "https://repo.mamba.pm",
        ],
    )
    def test_channel_alias(self, alias):
        if alias:
            res = update(
                "-n",
                TestUpdate.env_name,
                "xtensor",
                "--json",
                "--dry-run",
                "--channel-alias",
                alias,
            )
            ca = alias.rstrip("/")
        else:
            res = update("-n", TestUpdate.env_name, "xtensor", "--json", "--dry-run")
            ca = "https://conda.anaconda.org"

        for l in res["actions"]["LINK"]:
            assert l["channel"].startswith(f"{ca}/conda-forge/")
            assert l["url"].startswith(f"{ca}/conda-forge/")

    @pytest.mark.parametrize("channels", [None, "both", "CLI_only", "file_only"])
    @pytest.mark.parametrize("env_name", [None, "both", "CLI_only", "file_only"])
    @pytest.mark.parametrize("specs", ["both", "CLI_only", "file_only"])
    def test_yaml_spec_file(self, channels, env_name, specs):
        spec_file_content = []
        if env_name not in ("CLI_only", None):
            spec_file_content += ["name: not_existing_env"]
        if channels not in ("CLI_only", None):
            spec_file_content += ["channels:", "  - https://repo.mamba.pm/conda-forge"]
        if specs != "CLI_only":
            spec_file_content += ["dependencies:", "  - xtensor"]

        yaml_spec_file = os.path.join(TestUpdate.prefix, "yaml_env.yml")
        with open(yaml_spec_file, "w") as f:
            f.write("\n".join(spec_file_content))

        cmd = []

        if specs != "file_only":
            cmd += ["xtl"]

        if env_name not in ("file_only", None):
            cmd += [
                "-n",
                TestUpdate.env_name,
            ]

        if channels not in ("file_only", None):
            cmd += ["-c", "https://conda.anaconda.org/conda-forge"]

        cmd += ["-f", yaml_spec_file, "--json"]

        if specs == "CLI_only" or env_name == "file_only":
            with pytest.raises(subprocess.CalledProcessError):
                update(*cmd, default_channel=False)
        else:
            res = update(*cmd, default_channel=False)

            assert res["success"]
            assert not res["dry_run"]
            assert res["prefix"] == TestUpdate.prefix

            if channels is None:
                # No channels currently only warn on update sub command
                keys = {"success", "prefix", "message", "dry_run"}
                assert keys.issubset(set(res.keys()))
                assert res["message"] == "All requested packages already installed"
            else:
                keys = {"success", "prefix", "actions", "dry_run"}
                assert keys.issubset(set(res.keys()))

                action_keys = {"LINK", "PREFIX"}
                assert action_keys.issubset(set(res["actions"].keys()))

                pkg = get_concrete_pkg(res, "xtensor")
                pkg_info = get_concrete_pkg_info(get_env(TestUpdate.env_name), pkg)
                version = pkg_info["version"]

                assert TestUpdate.old_version != version

                if channels == "file_only":
                    expected_channel = "https://repo.mamba.pm/conda-forge"
                else:
                    expected_channel = "https://conda.anaconda.org/conda-forge"

                for l in res["actions"]["LINK"]:
                    assert l["channel"].startswith(expected_channel)
                    assert l["url"].startswith(expected_channel)
