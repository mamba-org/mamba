import platform
import subprocess
import sys

import pytest

from . import helpers


@pytest.mark.parametrize("reverse_flag", ["", "--reverse"])
@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list(tmp_home, tmp_root_prefix, tmp_env_name, tmp_xtensor_env, env_selector, quiet_flag, reverse_flag):
    if env_selector == "prefix":
        res = helpers.umamba_list("-p", tmp_xtensor_env, "--json", quiet_flag, reverse_flag)
    elif env_selector == "name":
        res = helpers.umamba_list("-n", tmp_env_name, "--json", quiet_flag, reverse_flag)
    else:
        res = helpers.umamba_list("--json", quiet_flag, reverse_flag)

    assert len(res) > 2

    names = [i["name"] for i in res]
    assert "xtensor" in names
    assert "xtl" in names
    assert all(
        i["channel"] == "conda-forge" and i["base_url"] == "https://conda.anaconda.org/conda-forge"
        for i in res
    )

    if reverse_flag == "--reverse":
        assert names.index("xtensor") > names.index("xtl")
    else:
        assert names.index("xtensor") < names.index("xtl")


@pytest.mark.parametrize("reverse_flag", ["", "--reverse"])
@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_no_json(tmp_home, tmp_root_prefix, tmp_env_name, tmp_xtensor_env, env_selector, quiet_flag, reverse_flag):
    if env_selector == "prefix":
        res = helpers.umamba_list("-p", tmp_xtensor_env, quiet_flag, reverse_flag)
    elif env_selector == "name":
        res = helpers.umamba_list("-n", tmp_env_name, quiet_flag, reverse_flag)
    else:
        res = helpers.umamba_list(quiet_flag, reverse_flag)

    assert len(res) > 10

    assert "xtensor" in res
    assert "xtl" in res

    assert len(res.split("\n")) - 5 == res.count("conda-forge")

    if reverse_flag == "--reverse":
        assert res.find("xtensor") > res.find("xtl")
    else:
        assert res.find("xtensor") < res.find("xtl")


@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_name(tmp_home, tmp_root_prefix, tmp_xtensor_env, quiet_flag):
    helpers.install("xtensor-python")
    res = helpers.umamba_list("xt", "--json", quiet_flag)
    names = sorted([i["name"] for i in res])
    assert names == ["xtensor", "xtensor-python", "xtl"]

    full_res = helpers.umamba_list("xtensor", "--full-name", "--json", quiet_flag)
    full_names = sorted([i["name"] for i in full_res])
    assert full_names == ["xtensor"]


env_yaml_content_to_install_numpy_with_pip = """
channels:
- conda-forge
dependencies:
- pip
- pip:
  - numpy==1.26.4
"""


@pytest.mark.parametrize("no_pip_flag", ["", "--no-pip"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list_with_pip(tmp_home, tmp_root_prefix, tmp_path, no_pip_flag):
    env_name = "env-list_with_pip"
    tmp_root_prefix / "envs" / env_name

    env_file_yml = tmp_path / "test_env_yaml_content_to_install_numpy_with_pip.yaml"
    env_file_yml.write_text(env_yaml_content_to_install_numpy_with_pip)

    helpers.create("-n", env_name, "python=3.12", "--json", no_dry_run=True)
    helpers.install("-n", env_name, "-f", env_file_yml, "--json", no_dry_run=True)

    res = helpers.umamba_list("-n", env_name, "--json", no_pip_flag)
    if no_pip_flag == "":
        assert any(
            package["name"] == "numpy"
            and package["version"] == "1.26.4"
            and package["base_url"] == "https://pypi.org/"
            and package["build_string"] == "pypi_0"
            and package["channel"] == "pypi"
            and package["platform"] == sys.platform + "-" + platform.machine()
            for package in res
        )
    else:  # --no-pip
        # Check that numpy installed with pip is not listed
        assert all(package["name"] != "numpy" for package in res)


@pytest.mark.parametrize("env_selector", ["name", "prefix"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_not_existing(tmp_home, tmp_root_prefix, tmp_xtensor_env, env_selector):
    if env_selector == "prefix":
        cmd = ("-p", tmp_root_prefix / "envs" / "does-not-exist", "--json")
    elif env_selector == "name":
        cmd = ("-n", "does-not-exist", "--json")

    with pytest.raises(subprocess.CalledProcessError):
        helpers.umamba_list(*cmd)


def test_not_environment(tmp_home, tmp_root_prefix):
    with pytest.raises(subprocess.CalledProcessError):
        helpers.umamba_list("-p", tmp_root_prefix / "envs", "--json")


@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_regex(tmp_home, tmp_root_prefix, tmp_xtensor_env, quiet_flag):
    full_res = helpers.umamba_list("--json")
    names = sorted([i["name"] for i in full_res])

    filtered_res = helpers.umamba_list("\\**", "--json", quiet_flag)
    filtered_names = sorted([i["name"] for i in filtered_res])
    assert filtered_names == names

    filtered_res = helpers.umamba_list("^xt", "--json", quiet_flag)
    filtered_names = sorted([i["name"] for i in filtered_res])
    assert filtered_names == ["xtensor", "xtl"]
