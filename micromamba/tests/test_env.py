import os
import re
import shutil
import subprocess
import platform

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


def test_env_list_multiple_envs_dirs(tmp_home, tmp_root_prefix, tmp_path):
    """Test that 'mamba env list' shows names for environments in all envs_dirs directories.

    This is a regression test for issue #4045 where only environments in the first
    envs_dirs directory would have their names displayed.
    """
    # Create two separate envs_dirs directories
    envs_dir1 = tmp_path / "envs1"
    envs_dir2 = tmp_path / "envs2"
    envs_dir1.mkdir()
    envs_dir2.mkdir()

    # Create environments in both directories
    env1_name = "env1"
    env2_name = "env2"
    env1_path = envs_dir1 / env1_name
    env2_path = envs_dir2 / env2_name

    helpers.create("-p", env1_path, "--offline", "--no-rc", no_dry_run=True)
    helpers.create("-p", env2_path, "--offline", "--no-rc", no_dry_run=True)

    # Configure envs_dirs in .condarc with both directories
    with open(tmp_home / ".condarc", "w+") as f:
        f.write(f"envs_dirs:\n  - {envs_dir1}\n  - {envs_dir2}\n")

    # Run env list and check output
    res = helpers.run_env("list", "--rc-file", tmp_home / ".condarc")

    # Both environments should have their names displayed, not just paths
    assert env1_name in res
    assert env2_name in res
    assert str(env1_path) in res
    assert str(env2_path) in res

    # Parse the output to verify names are in the Name column
    lines = res.splitlines()
    name_col_idx = None
    for i, line in enumerate(lines):
        if "Name" in line and "Path" in line:
            # Find the column positions
            name_col_idx = line.find("Name")
            break

    if name_col_idx is not None:
        # Check that both env names appear in the Name column
        found_env1_name = False
        found_env2_name = False
        for line in lines:
            if line.strip().startswith(env1_name):
                # The line should start with the env name (in Name column)
                found_env1_name = True
            if line.strip().startswith(env2_name):
                found_env2_name = True

        assert found_env1_name, f"Environment {env1_name} name not found in Name column"
        assert found_env2_name, f"Environment {env2_name} name not found in Name column"


@pytest.fixture(scope="module")
def empty_env():
    env_name = "env-empty"
    helpers.create("-n", env_name)
    return env_name


@pytest.mark.parametrize("json_flag", [None, "--json"])
def test_env_export_empty(json_flag, empty_env):
    flags = filter(None, [json_flag])
    output = helpers.run_env("export", "-n", empty_env, *flags)

    # json is already parsed
    ret = output if json_flag else yaml.safe_load(output)
    assert ret["name"] == empty_env
    assert empty_env in ret["prefix"]
    assert not ret["channels"]


@pytest.fixture(scope="module")
def export_env():
    env_name = "env-create-export"
    spec_file = __this_dir__ / "env-create-export.yaml"
    helpers.create("-n", env_name, "-f", spec_file)
    return env_name


@pytest.mark.parametrize("json_flag", [None, "--json"])
def test_env_export_from_history(json_flag, export_env):
    flags = filter(None, [json_flag])
    output = helpers.run_env("export", "-n", export_env, "--from-history", *flags)

    # json is already parsed
    ret = output if json_flag else yaml.safe_load(output)
    assert ret["name"] == export_env
    assert export_env in ret["prefix"]
    assert set(ret["channels"]) == {"conda-forge"}
    micromamba_spec_prefix = "micromamba=0.24.0"
    assert [micromamba_spec_prefix] == ret["dependencies"]


@pytest.mark.parametrize("channel_subdir_flag", [None, "--channel-subdir"])
@pytest.mark.parametrize("md5_flag", [None, "--md5", "--no-md5"])
@pytest.mark.parametrize("explicit_flag", [None, "--explicit"])
@pytest.mark.parametrize("no_build_flag", [None, "--no-build", "--no-builds"])
@pytest.mark.parametrize("json_flag", [None, "--json"])
def test_env_export(
    channel_subdir_flag, md5_flag, explicit_flag, no_build_flag, json_flag, export_env
):
    if explicit_flag and json_flag:
        # `--explicit` has precedence over `--json`, which is tested bellow.
        # But we need to omit here to avoid `helpers.run_env` to parse the output as JSON and fail.
        json_flag = None

    flags = filter(None, [channel_subdir_flag, md5_flag, explicit_flag, no_build_flag, json_flag])
    output = helpers.run_env("export", "-n", export_env, *flags)
    if explicit_flag:
        assert "/micromamba-0.24.0-0." in output
        if md5_flag != "--no-md5":
            assert re.search("#[a-f0-9]{32}$", output.replace("\r", ""))
    else:
        # json is already parsed
        ret = output if json_flag else yaml.safe_load(output)
        assert ret["name"] == export_env
        assert export_env in ret["prefix"]
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
    with open(conda_env_file, encoding="utf-8") as f:
        lines = [line.strip() for line in f]
        assert str(env_fp) in lines

    # Unregister / remove env_name
    res = helpers.run_env("remove", "-n", env_name, "-y")
    assert "To activate this environment, use:" not in res

    env_json = helpers.run_env("list", "--json")
    assert str(env_fp) not in env_json["envs"]
    assert not env_fp.exists()
    with open(conda_env_file, encoding="utf-8") as f:
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

    # Install numpy from conda-forge instead of using env update
    # Disable interoperability to allow both pip and conda versions to coexist
    res = helpers.install(
        "numpy=2.0.0",
        "-p",
        env_prefix,
        "-y",
        "--json",
        env={"CONDA_PREFIX_DATA_INTEROPERABILITY": "false"},
    )
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


env_yaml_content_env_export_with_pip = """
channels:
- conda-forge
dependencies:
- pip
- pip:
  - requests==2.32.3
"""


@pytest.mark.parametrize("json_flag", [None, "--json"])
def test_env_export_with_pip(tmp_path, json_flag):
    env_name = "env_export_with_pip"

    env_file_yml = tmp_path / "test_env_yaml_content_to_install_requests_with_pip.yaml"
    env_file_yml.write_text(env_yaml_content_env_export_with_pip)

    flags = list(filter(None, [json_flag]))
    helpers.create("-n", env_name, "-f", env_file_yml, no_dry_run=True)

    output = helpers.run_env("export", "-n", env_name, *flags)

    # JSON is already parsed
    ret = output if json_flag else yaml.safe_load(output)

    assert ret["name"] == env_name
    assert env_name in ret["prefix"]
    assert set(ret["channels"]) == {"conda-forge"}

    pip_section = next(
        dep for dep in ret["dependencies"] if isinstance(dep, dict) and ["pip"] == [*dep]
    )
    pip_section_vals = pip_section["pip"]

    # Check that `requests` and `urllib3` (pulled dependency) are exported
    assert "requests==2.32.3" in pip_section_vals
    assert any(pkg.startswith("urllib3==") for pkg in pip_section_vals)


env_yaml_content_env_export_with_uv_flag = """
channels:
- conda-forge
dependencies:
- python
- pip:
  - requests==2.32.3
"""


@pytest.mark.parametrize("json_flag", [None, "--json"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_env_export_with_uv_flag(tmp_home, tmp_root_prefix, tmp_path, json_flag):
    """Test that environment export with --use-uv flag works correctly."""
    env_name = "env_export_with_uv_flag"

    env_file_yml = tmp_path / "test_env_yaml_content_to_install_requests_with_uv_flag.yaml"
    env_file_yml.write_text(env_yaml_content_env_export_with_uv_flag)

    flags = list(filter(None, [json_flag]))
    helpers.create("-n", env_name, "-f", env_file_yml, "--use-uv", no_dry_run=True)

    output = helpers.run_env("export", "-n", env_name, *flags)

    # JSON is already parsed
    ret = output if json_flag else yaml.safe_load(output)

    assert ret["name"] == env_name
    assert env_name in ret["prefix"]
    assert set(ret["channels"]) == {"conda-forge"}

    pip_section = next(
        dep for dep in ret["dependencies"] if isinstance(dep, dict) and ["pip"] == [*dep]
    )
    pip_section_vals = pip_section["pip"]

    # Check that `requests` and `urllib3` (pulled dependency) are exported
    assert "requests==2.32.3" in pip_section_vals
    assert any(pkg.startswith("urllib3==") for pkg in pip_section_vals)


def test_env_export_with_ca_certificates(tmp_path):
    # CA certificates in the same environment as `mamba` or `micromamba`
    # executable installation are used by default.
    tmp_env_prefix = tmp_path / "env-export-with-ca-certificates"

    helpers.create("-p", tmp_env_prefix, "ca-certificates", no_dry_run=True)

    # Copy the `mamba` or `micromamba` executable in this prefix `bin` subdirectory
    built_executable = helpers.get_umamba()
    executable_basename = os.path.basename(built_executable)

    if platform.system() == "Windows":
        tmp_env_bin_dir = tmp_env_prefix / "Library" / "bin"
        tmp_env_executable = tmp_env_bin_dir / executable_basename
        (tmp_env_bin_dir).mkdir(parents=True, exist_ok=True)
        # Copy all the `Library/bin/` subdirectory to have the executable in
        # the environment and the DLLs in the environment.
        shutil.copytree(Path(built_executable).parent, tmp_env_bin_dir, dirs_exist_ok=True)
    else:
        tmp_env_bin_dir = tmp_env_prefix / "bin"
        tmp_env_executable = tmp_env_bin_dir / executable_basename
        (tmp_env_bin_dir).mkdir(parents=True, exist_ok=True)
        shutil.copy(built_executable, tmp_env_executable)

    # Run a command using mamba in verbose mode and check that the ca-certificates file
    # from the same prefix as the executable is used by default.
    p = subprocess.run(
        [tmp_env_executable, "search", "xtensor", "-v"],
        capture_output=True,
        check=False,
    )
    stderr = p.stderr.decode()
    assert (
        "Checking for CA certificates in the same prefix as the executable installation" in stderr
    )
    assert (
        "Using CA certificates from `conda-forge::ca-certificates` installed in the same prefix"
        in stderr
    )


def test_env_config_vars_list_empty(tmp_home, tmp_root_prefix):
    """Test listing environment variables when none are set."""
    env_name = "env-vars-list-empty"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    output = helpers.run_env("config", "vars", "list", "-n", env_name)
    assert "No environment variables set" in output


def test_env_config_vars_list_json(tmp_home, tmp_root_prefix):
    """Test listing environment variables in JSON format."""
    env_name = "env-vars-list-json"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    output = helpers.run_env("config", "vars", "list", "-n", env_name, "--json")
    assert "env_vars" in output
    assert output["env_vars"] == {}


def test_env_config_vars_list_format(tmp_home, tmp_root_prefix):
    """Test that list output matches conda format: 'KEY = VALUE' and preserves insertion order."""
    env_name = "env-vars-list-format"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    # Set environment variables in specific order
    helpers.run_env("config", "vars", "set", "-n", env_name, "A=B", "C=D", "MY_VAR=my_value")

    # List and verify format matches conda: "KEY = VALUE"
    output = helpers.run_env("config", "vars", "list", "-n", env_name)
    lines = [line.strip() for line in output.strip().splitlines() if line.strip()]

    # Should have 3 lines in format "KEY = VALUE"
    assert len(lines) == 3

    # Verify format: each line should match "KEY = VALUE" pattern
    expected_lines = {"A = B", "C = D", "MY_VAR = my_value"}
    assert set(lines) == expected_lines

    # Verify order matches insertion order (not alphabetical)
    # Order should be A, C, MY_VAR (as they were set)
    assert lines[0] == "A = B"
    assert lines[1] == "C = D"
    assert lines[2] == "MY_VAR = my_value"


def test_env_config_vars_list_preserves_insertion_order(tmp_home, tmp_root_prefix):
    """Test that list preserves insertion order, not alphabetical order."""
    env_name = "env-vars-order"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    # Set variables in NON-alphabetical order: Z, A, M
    # Alphabetically would be: A, M, Z
    # But insertion order should be preserved: Z, A, M
    helpers.run_env("config", "vars", "set", "-n", env_name, "Z=z_value", "A=a_value", "M=m_value")

    # Verify order matches insertion order (Z, A, M), NOT alphabetical (A, M, Z)
    output = helpers.run_env("config", "vars", "list", "-n", env_name)
    lines = [line.strip() for line in output.strip().splitlines() if line.strip()]
    assert lines == ["Z = z_value", "A = a_value", "M = m_value"]

    # Verify it's NOT alphabetical
    assert lines != sorted(lines), "Variables should be in insertion order, not alphabetical"

    # Test with a different permutation of the same variables
    # Remove all and set in different order: M, Z, A
    helpers.run_env("config", "vars", "unset", "-n", env_name, "Z", "A", "M")
    helpers.run_env("config", "vars", "set", "-n", env_name, "M=m_value", "Z=z_value", "A=a_value")

    # Verify order changed to match new insertion order (M, Z, A)
    output = helpers.run_env("config", "vars", "list", "-n", env_name)
    lines = [line.strip() for line in output.strip().splitlines() if line.strip()]
    assert lines == ["M = m_value", "Z = z_value", "A = a_value"]

    # Verify it's different from previous order
    assert lines != ["Z = z_value", "A = a_value", "M = m_value"], (
        "Order should change with different insertion order"
    )

    # Test adding a new variable (should appear at the end)
    helpers.run_env("config", "vars", "set", "-n", env_name, "I=i_value")

    # Verify new variable appears at end, preserving previous order
    output = helpers.run_env("config", "vars", "list", "-n", env_name)
    lines = [line.strip() for line in output.strip().splitlines() if line.strip()]
    assert lines == ["M = m_value", "Z = z_value", "A = a_value", "I = i_value"]


def test_env_config_vars_set_and_list(tmp_home, tmp_root_prefix):
    """Test setting and listing environment variables."""
    env_name = "env-vars-set-list"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    # Set environment variables
    helpers.run_env(
        "config", "vars", "set", "-n", env_name, "MY_VAR=my_value", "OTHER_VAR=other_value"
    )

    # List and verify JSON format
    output = helpers.run_env("config", "vars", "list", "-n", env_name, "--json")
    assert "env_vars" in output
    assert output["env_vars"]["MY_VAR"] == "my_value"
    assert output["env_vars"]["OTHER_VAR"] == "other_value"

    # List and verify text format matches conda: "KEY = VALUE"
    text_output = helpers.run_env("config", "vars", "list", "-n", env_name)
    lines = [line.strip() for line in text_output.strip().splitlines() if line.strip()]
    assert len(lines) == 2
    assert "MY_VAR = my_value" in lines
    assert "OTHER_VAR = other_value" in lines


def test_env_config_vars_set_case_insensitive(tmp_home, tmp_root_prefix):
    """Test that environment variable keys are case-insensitive (stored uppercase)."""
    env_name = "env-vars-case"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    # Set with lowercase
    helpers.run_env("config", "vars", "set", "-n", env_name, "my_var=value1")

    # Set with uppercase (should overwrite)
    helpers.run_env("config", "vars", "set", "-n", env_name, "MY_VAR=value2")

    # List and verify only one entry exists (uppercase)
    output = helpers.run_env("config", "vars", "list", "-n", env_name, "--json")
    assert "env_vars" in output
    assert output["env_vars"]["MY_VAR"] == "value2"
    assert "my_var" not in output["env_vars"]


def test_env_config_vars_unset(tmp_home, tmp_root_prefix):
    """Test unsetting environment variables."""
    env_name = "env-vars-unset"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    # Set environment variables
    helpers.run_env("config", "vars", "set", "-n", env_name, "VAR1=value1", "VAR2=value2")

    # Unset one
    helpers.run_env("config", "vars", "unset", "-n", env_name, "VAR1")

    # List and verify
    output = helpers.run_env("config", "vars", "list", "-n", env_name, "--json")
    assert "env_vars" in output
    assert "VAR1" not in output["env_vars"]
    assert output["env_vars"]["VAR2"] == "value2"


def test_env_config_vars_unset_case_insensitive(tmp_home, tmp_root_prefix):
    """Test that unset is case-insensitive."""
    env_name = "env-vars-unset-case"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    # Set with uppercase
    helpers.run_env("config", "vars", "set", "-n", env_name, "MY_VAR=value")

    # Unset with lowercase
    helpers.run_env("config", "vars", "unset", "-n", env_name, "my_var")

    # List and verify it's gone
    output = helpers.run_env("config", "vars", "list", "-n", env_name, "--json")
    assert "env_vars" in output
    assert "MY_VAR" not in output["env_vars"]


def test_env_config_vars_set_multiple(tmp_home, tmp_root_prefix):
    """Test setting multiple environment variables at once."""
    env_name = "env-vars-multiple"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    # Set multiple variables
    helpers.run_env(
        "config",
        "vars",
        "set",
        "-n",
        env_name,
        "VAR1=value1",
        "VAR2=value2",
        "VAR3=value3",
    )

    # List and verify all are set
    output = helpers.run_env("config", "vars", "list", "-n", env_name, "--json")
    assert "env_vars" in output
    assert len(output["env_vars"]) == 3
    assert output["env_vars"]["VAR1"] == "value1"
    assert output["env_vars"]["VAR2"] == "value2"
    assert output["env_vars"]["VAR3"] == "value3"


def test_env_config_vars_set_with_equals_in_value(tmp_home, tmp_root_prefix):
    """Test setting environment variable with equals sign in value."""
    env_name = "env-vars-equals"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    # Set variable with equals in value (only first = is the separator)
    helpers.run_env("config", "vars", "set", "-n", env_name, "PATH=/usr/bin:/usr/local/bin")

    # List and verify
    output = helpers.run_env("config", "vars", "list", "-n", env_name, "--json")
    assert "env_vars" in output
    assert output["env_vars"]["PATH"] == "/usr/bin:/usr/local/bin"


def test_env_config_vars_state_file_updates(tmp_home, tmp_root_prefix):
    """Test that the state file is correctly updated for addition, update, and deletion."""
    import json

    env_name = "env-vars-state-file"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    env_prefix = tmp_root_prefix / "envs" / env_name
    state_file_path = env_prefix / "conda-meta" / "state"

    # Initially, state file should not exist or be empty
    if state_file_path.exists():
        with open(state_file_path) as f:
            state_content = f.read().strip()
            if state_content:
                state = json.loads(state_content)
                assert "env_vars" not in state or state["env_vars"] == {}
    else:
        # State file doesn't exist yet, which is fine
        pass

    # Step 1: Add first variable (A=B)
    helpers.run_env("config", "vars", "set", "-n", env_name, "A=B")

    # Verify state file after addition
    helpers.assert_state_file(state_file_path, {"env_vars": {"A": "B"}})

    # Step 2: Add second variable (C=D)
    helpers.run_env("config", "vars", "set", "-n", env_name, "C=D")

    # Verify state file after second addition
    helpers.assert_state_file(state_file_path, {"env_vars": {"A": "B", "C": "D"}})

    # Step 3: Update existing variable (A=E)
    helpers.run_env("config", "vars", "set", "-n", env_name, "A=E")

    # Verify state file after update
    helpers.assert_state_file(state_file_path, {"env_vars": {"A": "E", "C": "D"}})

    # Step 4: Add third variable (F=G)
    helpers.run_env("config", "vars", "set", "-n", env_name, "F=G")

    # Verify state file after third addition
    helpers.assert_state_file(state_file_path, {"env_vars": {"A": "E", "C": "D", "F": "G"}})

    # Step 5: Delete variable C
    helpers.run_env("config", "vars", "unset", "-n", env_name, "C")

    # Verify state file after deletion
    helpers.assert_state_file(state_file_path, {"env_vars": {"A": "E", "F": "G"}})

    # Step 6: Delete variable A
    helpers.run_env("config", "vars", "unset", "-n", env_name, "A")

    # Verify state file after second deletion
    helpers.assert_state_file(state_file_path, {"env_vars": {"F": "G"}})

    # Step 7: Delete last variable F
    helpers.run_env("config", "vars", "unset", "-n", env_name, "F")

    # Verify state file after final deletion - should have empty env_vars
    helpers.assert_state_file(state_file_path, {"env_vars": {}})


def test_env_config_vars_state_file_preserves_other_fields(tmp_home, tmp_root_prefix):
    """Test that updating env vars preserves other fields in the state file."""
    import json

    env_name = "env-vars-preserve"
    helpers.create("-n", env_name, "--json", no_dry_run=True)

    env_prefix = tmp_root_prefix / "envs" / env_name
    state_file_path = env_prefix / "conda-meta" / "state"

    # Create state file with other fields
    initial_state = {"some_other_field": "some_value", "another_field": 123, "env_vars": {}}
    state_file_path.parent.mkdir(parents=True, exist_ok=True)
    with open(state_file_path, "w") as f:
        json.dump(initial_state, f)

    # Set an environment variable
    helpers.run_env("config", "vars", "set", "-n", env_name, "TEST_VAR=test_value")

    # Verify that other fields are preserved
    helpers.assert_state_file(
        state_file_path,
        {
            "env_vars": {"TEST_VAR": "test_value"},
            "some_other_field": "some_value",
            "another_field": 123,
        },
    )

    # Update the variable
    helpers.run_env("config", "vars", "set", "-n", env_name, "TEST_VAR=updated_value")

    # Verify other fields are still preserved
    helpers.assert_state_file(
        state_file_path,
        {
            "env_vars": {"TEST_VAR": "updated_value"},
            "some_other_field": "some_value",
            "another_field": 123,
        },
    )

    # Unset the variable
    helpers.run_env("config", "vars", "unset", "-n", env_name, "TEST_VAR")

    # Verify other fields are still preserved
    helpers.assert_state_file(
        state_file_path,
        {
            "env_vars": {},
            "some_other_field": "some_value",
            "another_field": 123,
        },
    )
