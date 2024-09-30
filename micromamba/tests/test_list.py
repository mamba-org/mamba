import subprocess

import pytest

from . import helpers


@pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
@pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_list(tmp_home, tmp_root_prefix, tmp_env_name, tmp_xtensor_env, env_selector, quiet_flag):
    if env_selector == "prefix":
        res = helpers.umamba_list("-p", tmp_xtensor_env, "--json", quiet_flag)
    elif env_selector == "name":
        res = helpers.umamba_list("-n", tmp_env_name, "--json", quiet_flag)
    else:
        res = helpers.umamba_list("--json", quiet_flag)

    assert len(res) > 2

    names = [i["name"] for i in res]
    assert "xtensor" in names
    assert "xtl" in names
    assert all(
        i["channel"] == "conda-forge" and i["base_url"] == "https://conda.anaconda.org/conda-forge"
        for i in res
    )


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
