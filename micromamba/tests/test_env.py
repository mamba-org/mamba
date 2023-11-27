import os
import re
import shutil
from pathlib import Path

import pytest
import yaml

from . import helpers

__this_dir__ = Path(__file__).parent.resolve()


def test_env_list(tmp_home, tmp_root_prefix, tmp_empty_env):
    env_json = helpers.run_env("list", "--json")

    assert "envs" in env_json
    assert len(env_json["envs"]) >= 2
    assert str(tmp_root_prefix) in env_json["envs"]
    assert str(tmp_empty_env) in env_json["envs"]


def test_env_list_table(tmp_home, tmp_root_prefix, tmp_prefix):
    res = helpers.run_env("list")

    assert "Name" in res
    assert "base" in res
    assert str(tmp_root_prefix) in res
    all_lines = res.splitlines()
    print("\n".join(all_lines))
    for line in all_lines:
        if "*" in line:
            active_env_l = line
    assert str(tmp_root_prefix) in active_env_l

    os.environ["CONDA_PREFIX"] = str(tmp_prefix)

    res = helpers.run_env("list")

    all_lines = res.splitlines()
    for line in all_lines:
        if "*" in line:
            active_env_l = line
    assert str(tmp_prefix) in active_env_l


def test_register_new_env(tmp_home, tmp_root_prefix):
    helpers.create("-n", "env2", "--json", no_dry_run=True)
    helpers.create("-n", "env3", "--json", no_dry_run=True)

    env_json = helpers.run_env("list", "--json")
    env_2_fp = tmp_root_prefix / "envs" / "env2"
    env_3_fp = tmp_root_prefix / "envs" / "env3"
    assert str(env_2_fp) in env_json["envs"]
    assert str(env_3_fp) in env_json["envs"]

    shutil.rmtree(env_2_fp)
    env_json = helpers.run_env("list", "--json")
    assert str(env_2_fp) not in env_json["envs"]
    assert str(env_3_fp) in env_json["envs"]


@pytest.fixture(scope="module")
def export_env():
    env_name = "env-create-export"
    spec_file = __this_dir__ / "env-create-export.yaml"
    helpers.create("-n", env_name, "-f", spec_file)
    return env_name


@pytest.mark.parametrize("channel_subdir_flag", [None, "--channel-subdir"])
@pytest.mark.parametrize("md5_flag", [None, "--md5", "--no-md5"])
@pytest.mark.parametrize("explicit_flag", [None, "--explicit"])
def test_env_export(export_env, explicit_flag, md5_flag, channel_subdir_flag):
    flags = filter(None, [explicit_flag, md5_flag, channel_subdir_flag])
    output = helpers.run_env("export", "-n", export_env, *flags)
    if explicit_flag:
        assert "/micromamba-0.24.0-0." in output
        if md5_flag != "--no-md5":
            assert re.search("#[a-f0-9]{32}$", output.replace("\r", ""))
    else:
        ret = yaml.safe_load(output)
        assert ret["name"] == export_env
        assert set(ret["channels"]) == {"conda-forge"}
        assert "micromamba=0.24.0=0" in str(ret["dependencies"])
        if md5_flag == "--md5":
            assert re.search(
                r"micromamba=0.24.0=0\[md5=[a-f0-9]{32}\]", str(ret["dependencies"])
            )
        if channel_subdir_flag:
            assert re.search(
                r"conda-forge/[a-z0-9-]+::micromamba=0.24.0=0", str(ret["dependencies"])
            )


def test_create():
    """Tests for ``micromamba env create`` can be found in ``test_create.py``.

    Look for 'create_cmd'.
    """
    pass


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_remove(tmp_home, tmp_root_prefix):
    env_name = "env-create-remove"
    env_fp = tmp_root_prefix / "envs" / env_name
    conda_env_file = tmp_home / ".conda/environments.txt"

    # Create env with xtensor
    helpers.create("xtensor", "-n", env_name, "--json", no_dry_run=True)

    env_json = helpers.run_env("list", "--json")
    assert str(env_fp) in env_json["envs"]
    assert env_fp.exists()
    with open(conda_env_file, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f]
        assert str(env_fp) in lines

    # Unregister / remove env_name
    helpers.run_env("remove", "-n", env_name, "-y")
    env_json = helpers.run_env("list", "--json")
    assert str(env_fp) not in env_json["envs"]
    assert not env_fp.exists()
    with open(conda_env_file, "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f]
        assert str(env_fp) not in lines


env_yaml_content = """
channels:
- conda-forge
dependencies:
- python
"""


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("prune", (False, True))
def test_env_update(tmp_home, tmp_root_prefix, tmp_path, prune):
    env_name = "env-create-update"

    # Create env with python=3.6.15 and xtensor=0.20.0
    helpers.create(
        "python=3.6.15", "xtensor=0.20.0", "-n", env_name, "--json", no_dry_run=True
    )
    packages = helpers.umamba_list("-n", env_name, "--json")
    assert any(
        package["name"] == "python" and package["version"] == "3.6.15"
        for package in packages
    )
    assert any(
        package["name"] == "xtensor" and package["version"] == "0.20.0"
        for package in packages
    )
    assert any(package["name"] == "xtl" for package in packages)

    # Update python
    from packaging.version import Version

    env_file_yml = tmp_path / "test_env.yaml"
    env_file_yml.write_text(env_yaml_content)

    cmd = ["update", "-n", env_name, f"--file={env_file_yml}", "-y"]
    if prune:
        cmd += ["--prune"]
    helpers.run_env(*cmd)
    packages = helpers.umamba_list("-n", env_name, "--json")
    assert any(
        package["name"] == "python" and Version(package["version"]) > Version("3.6.15")
        for package in packages
    )
    if prune:
        assert not any(package["name"] == "xtensor" for package in packages)
        # Make sure dependencies of removed pkgs are removed as well (xtl is a dep of xtensor)
        assert not any(package["name"] == "xtl" for package in packages)
    else:
        assert any(
            package["name"] == "xtensor" and package["version"] == "0.20.0"
            for package in packages
        )
        assert any(package["name"] == "xtl" for package in packages)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_explicit_export_topologically_sorted(tmp_home, tmp_prefix):
    """Explicit export must have dependencies before dependent packages."""
    helpers.install("python=3.10", "pip", "jupyterlab")
    lines = helpers.run_env("export", "--explicit").splitlines()

    indices = {
        "libzlib": 0,
        "python": 0,
        "wheel": 0,
        "pip": 0,
        "jupyterlab": 0,
    }
    for i, line in enumerate(lines):
        for pkg in indices.keys():
            if pkg in line:
                indices[pkg] = i

    assert indices["libzlib"] < indices["python"]
    assert indices["python"] < indices["wheel"]
    assert indices["wheel"] < indices["pip"]
    assert indices["python"] < indices["jupyterlab"]
