import os
import re
import shutil

from packaging.version import Version
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
@pytest.mark.parametrize("no_build_flag", [None, "--no-build", "--no-builds"])
@pytest.mark.parametrize("json_flag", [None, "--json"])
def test_env_export(
    export_env, json_flag, no_build_flag, explicit_flag, md5_flag, channel_subdir_flag
):
    if explicit_flag and json_flag:
        # `--explicit` has precedence over `--json`, which is tested bellow.
        # But we need to omit here to avoid `helpers.run_env` to parse the output as JSON and fail.
        json_flag = None

    flags = filter(None, [no_build_flag, json_flag, explicit_flag, md5_flag, channel_subdir_flag])
    output = helpers.run_env("export", "-n", export_env, *flags)
    if explicit_flag:
        assert "/micromamba-0.24.0-0." in output
        if md5_flag != "--no-md5":
            assert re.search("#[a-f0-9]{32}$", output.replace("\r", ""))
    else:
        if json_flag:
            # Already parsed
            ret = output
        else:
            ret = yaml.safe_load(output)
        assert ret["name"] == export_env
        assert "env-create-export" in ret["prefix"]
        assert set(ret["channels"]) == {"conda-forge"}
        micromamba_spec_prefix = "micromamba=0.24.0" if no_build_flag else "micromamba=0.24.0=0"
        assert micromamba_spec_prefix in str(ret["dependencies"])
        if md5_flag == "--md5" and no_build_flag:
            assert re.search(r"micromamba=0.24.0\[md5=[a-f0-9]{32}\]", str(ret["dependencies"]))
        if md5_flag == "--md5" and no_build_flag is None:
            assert re.search(r"micromamba=0.24.0=0\[md5=[a-f0-9]{32}\]", str(ret["dependencies"]))

        if channel_subdir_flag:
            assert re.search(r"conda-forge/[a-z0-9-]+::micromamba=0.24.0", str(ret["dependencies"]))


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


env_yaml_content_with_version_and_new_pkg = """
channels:
- conda-forge
dependencies:
- python 3.11.*
- ipython
"""


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("prune", (False, True))
def test_env_update(tmp_home, tmp_root_prefix, tmp_path, prune):
    env_name = "env-create-update"

    # Create env with python=3.10.0 and xtensor=0.25.0
    helpers.create("python=3.10.0", "xtensor=0.25.0", "-n", env_name, "--json", no_dry_run=True)
    packages = helpers.umamba_list("-n", env_name, "--json")
    assert any(
        package["name"] == "python" and package["version"] == "3.10.0" for package in packages
    )
    assert any(
        package["name"] == "xtensor" and package["version"] == "0.25.0" for package in packages
    )
    assert any(package["name"] == "xtl" for package in packages)
    assert not any(package["name"] == "ipython" for package in packages)

    # Update python
    env_file_yml = tmp_path / "test_env.yaml"
    env_file_yml.write_text(env_yaml_content_with_version_and_new_pkg)

    cmd = ["update", "-n", env_name, f"--file={env_file_yml}", "-y"]
    if prune:
        cmd += ["--prune"]
    helpers.run_env(*cmd)
    packages = helpers.umamba_list("-n", env_name, "--json")
    assert any(
        package["name"] == "python" and Version(package["version"]) >= Version("3.11.0")
        for package in packages
    )
    assert any(package["name"] == "ipython" for package in packages)
    if prune:
        assert not any(package["name"] == "xtensor" for package in packages)
        # Make sure dependencies of removed pkgs are removed as well
        # since `prune_deps` is always set to true in the case of `env update` command
        # (xtl is a dep of xtensor)
        assert not any(package["name"] == "xtl" for package in packages)
    else:
        assert any(
            package["name"] == "xtensor" and package["version"] == "0.25.0" for package in packages
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
        "pip": 0,
        "jupyterlab": 0,
    }
    for i, line in enumerate(lines):
        for pkg in indices.keys():
            if pkg in line:
                indices[pkg] = i

    assert indices["libzlib"] < indices["python"]
    assert indices["python"] < indices["pip"]
    assert indices["python"] < indices["jupyterlab"]


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_create_pypi(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "env-create-pypi"

    create_spec_file = tmp_path / "env-pypi-pkg-test.yaml"

    shutil.copyfile(__this_dir__ / "env-pypi-pkg-test.yaml", create_spec_file)

    res = helpers.run_env("create", "-p", env_prefix, "-f", create_spec_file, "-y", "--json")
    assert res["success"]

    # Check that pypi-pkg-test is installed using pip list for now
    # See: https://github.com/mamba-org/mamba/issues/2059
    pip_list_output = helpers.umamba_run("-p", env_prefix, "pip", "list", "--format=json")
    pip_packages_list = yaml.safe_load(pip_list_output)
    assert any(pkg["name"] == "pypi-pkg-test" for pkg in pip_packages_list)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_create_update_pypi(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "env-create-update-pypi"

    create_spec_file = tmp_path / "env-requires-pip-install.yaml"
    update_spec_file = tmp_path / "env-pypi-pkg-test.yaml"

    shutil.copyfile(__this_dir__ / "env-requires-pip-install.yaml", create_spec_file)
    shutil.copyfile(__this_dir__ / "env-pypi-pkg-test.yaml", update_spec_file)

    res = helpers.run_env("create", "-p", env_prefix, "-f", create_spec_file, "-y", "--json")
    assert res["success"]

    # Check pip packages using pip list for now
    # See: https://github.com/mamba-org/mamba/issues/2059
    pip_list_output = helpers.umamba_run("-p", env_prefix, "pip", "list", "--format=json")
    pip_packages_list = yaml.safe_load(pip_list_output)

    assert any(pkg["name"] == "pydantic" for pkg in pip_packages_list)
    # Check that pypi-pkg-test is not installed before the update
    assert all(pkg["name"] != "pypi-pkg-test" for pkg in pip_packages_list)

    res = helpers.run_env("update", "-p", env_prefix, "-f", update_spec_file, "-y", "--json")
    assert res["success"]

    ## Check that pypi-pkg-test is installed after the update
    # (using pip list for now)
    # See: https://github.com/mamba-org/mamba/issues/2059
    pip_list_output = helpers.umamba_run("-p", env_prefix, "pip", "list", "--format=json")
    pip_packages_list = yaml.safe_load(pip_list_output)
    assert any(pkg["name"] == "pypi-pkg-test" for pkg in pip_packages_list)
    assert any(pkg["name"] == "pydantic" for pkg in pip_packages_list)


env_yaml_content_to_update_pip_pkg_version = """
channels:
- conda-forge
dependencies:
- pip
- pip:
  - numpy==1.24.3
"""


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_update_conda_forge_with_pypi(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "env-update-conda-forge-with-pypi"

    # Create env with numpy=1.24.2
    helpers.create("numpy=1.24.2", "-p", env_prefix, "--json", no_dry_run=True)
    packages = helpers.umamba_list("-p", env_prefix, "--json")
    assert any(
        package["name"] == "numpy"
        and package["version"] == "1.24.2"
        and package["channel"].startswith("conda-forge")
        for package in packages
    )

    env_file_yml = tmp_path / "test_env_update_pip_pkg_version.yaml"
    env_file_yml.write_text(env_yaml_content_to_update_pip_pkg_version)

    res = helpers.run_env("update", "-p", env_prefix, "-f", env_file_yml, "-y", "--json")
    assert res["success"]

    # Note that conda's behavior is different:
    # numpy 1.24.2 is uninstalled to be replaced with 1.24.3 from PyPI
    # (micro)mamba keeps both
    packages = helpers.umamba_list("-p", env_prefix, "--json")
    assert any(
        pkg["name"] == "numpy"
        and pkg["version"] == "1.24.2"
        and pkg["channel"].startswith("conda-forge")
        for pkg in packages
    )

    ## Check pip packages using pip list for now
    ## See: https://github.com/mamba-org/mamba/issues/2059
    pip_list_output = helpers.umamba_run("-p", env_prefix, "pip", "list", "--format=json")
    pip_packages_list = yaml.safe_load(pip_list_output)

    assert any(pkg["name"] == "numpy" and pkg["version"] == "1.24.3" for pkg in pip_packages_list)


env_yaml_content_create_pip_pkg_with_version = """
channels:
- conda-forge
dependencies:
# This version of Python covers all the versions of numpy available on conda-forge and PyPI for all platforms.
- python 3.12
- pip
- pip:
  - numpy==1.26.4
"""


env_yaml_content_to_update_pip_pkg_version_from_conda_forge = """
channels:
- conda-forge
dependencies:
- python 3.12  # Numpy has not been released for python >=3.13 yet
- numpy==2.0.0
"""


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_update_pypi_with_conda_forge(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "env-update-pypi-with-conda-forge"

    env_file_yml = tmp_path / "test_env_create_pip_pkg_with_version.yaml"
    env_file_yml.write_text(env_yaml_content_create_pip_pkg_with_version)

    # Create env with numpy=1.26.4
    res = helpers.run_env("create", "-p", env_prefix, "-f", env_file_yml, "-y", "--json")
    assert res["success"]

    ## Check pip packages using pip list for now
    ## See: https://github.com/mamba-org/mamba/issues/2059
    pip_list_output = helpers.umamba_run("-p", env_prefix, "pip", "list", "--format=json")
    pip_packages_list = yaml.safe_load(pip_list_output)

    assert any(pkg["name"] == "numpy" and pkg["version"] == "1.26.4" for pkg in pip_packages_list)

    env_file_yml = tmp_path / "test_env_update_pip_pkg_version_with_conda_forge.yaml"
    env_file_yml.write_text(env_yaml_content_to_update_pip_pkg_version_from_conda_forge)

    # Update numpy from conda-forge is not suppposed to be done
    res = helpers.run_env("update", "-p", env_prefix, "-f", env_file_yml, "-y", "--json")
    assert res["success"]

    # Note that conda's behavior is different:
    # numpy 2.0.0 is not installed and numpy 1.26.4 from PyPI is kept
    # (micro)mamba keeps both
    packages = helpers.umamba_list("-p", env_prefix, "--json")
    assert any(
        pkg["name"] == "numpy"
        and pkg["version"] == "2.0.0"
        and pkg["channel"].startswith("conda-forge")
        for pkg in packages
    )

    ## Check pip packages using pip list for now
    ## See: https://github.com/mamba-org/mamba/issues/2059
    pip_list_output = helpers.umamba_run("-p", env_prefix, "pip", "list", "--format=json")
    pip_packages_list = yaml.safe_load(pip_list_output)

    # When numpy 2.0.0 is installed using mamba,
    # `numpy-2.0.0.dist-info/` is still created in `env_prefix/lib/pythonx.x/site-packages/` alongside `numpy-1.26.4.dist-info`
    # therefore causing an unexpected result when listing the version.
    # In an ideal world, multiple package managers shouldn't be mixed but since this is supported, tests are here
    # (note that a warning is printed to the user in that case)
    assert any(
        pkg["name"] == "numpy" and Version(pkg["version"]) >= Version("1.26.4")
        for pkg in pip_packages_list
    )


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_create_whitespace(tmp_home, tmp_root_prefix, tmp_path):
    # Non-regression test for: https://github.com/mamba-org/mamba/issues/3453

    env_prefix = tmp_path / "env-extra-white-space"

    create_spec_file = tmp_path / "env-extra-white-space.yaml"

    shutil.copyfile(__this_dir__ / "env-extra-white-space.yaml", create_spec_file)

    res = helpers.run_env("create", "-p", env_prefix, "-f", create_spec_file, "-y", "--json")
    assert res["success"]

    # Check that the env was created
    assert env_prefix.exists()
    # Check that the env has the right packages
    packages = helpers.umamba_list("-p", env_prefix, "--json")

    assert any(
        package["name"] == "python" and Version(package["version"]) > Version("3.11")
        for package in packages
    )
    assert any(
        package["name"] == "numpy" and Version(package["version"]) < Version("2.0")
        for package in packages
    )
    assert any(
        package["name"] == "scipy"
        and Version("1.5.0") <= Version(package["version"]) < Version("2.0.0")
        for package in packages
    )
    assert any(
        package["name"] == "scikit-learn" and Version(package["version"]) > Version("1.0.0")
        for package in packages
    )


env_yaml_content_to_update_empty_base = """
channels:
- conda-forge
dependencies:
- python
- xtensor
"""


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_update_empty_base(tmp_home, tmp_root_prefix, tmp_path):
    env_prefix = tmp_path / "env-update-empty-base"

    os.environ["MAMBA_ROOT_PREFIX"] = str(env_prefix)

    env_file_yml = tmp_path / "test_env_empty_base.yaml"
    env_file_yml.write_text(env_yaml_content_to_update_empty_base)

    cmd = ["update", "-p", env_prefix, f"--file={env_file_yml}", "-y", "--json"]

    res = helpers.run_env(*cmd)
    assert res["success"]

    packages = helpers.umamba_list("-p", env_prefix, "--json")
    assert any(package["name"] == "xtensor" for package in packages)
    assert any(package["name"] == "python" for package in packages)
