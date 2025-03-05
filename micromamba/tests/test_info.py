import os

import pytest

from . import helpers


@pytest.mark.parametrize("prefix_selection", [None, "prefix", "name"])
def test_base(tmp_home, tmp_root_prefix, prefix_selection, monkeypatch):
    monkeypatch.setenv("CONDA_PREFIX", str(tmp_root_prefix))

    if prefix_selection == "prefix":
        infos = helpers.info("-p", tmp_root_prefix)
    elif prefix_selection == "name":
        infos = helpers.info("-n", "base")
    else:
        infos = helpers.info()

    assert "environment : base (active)" in infos
    assert f"env location : {tmp_root_prefix}" in infos
    assert f"user config files : {tmp_home / '.mambarc'}" in infos
    assert f"base environment : {tmp_root_prefix}" in infos


@pytest.mark.parametrize("prefix_selection", [None, "prefix", "name"])
def test_env(tmp_home, tmp_root_prefix, tmp_env_name, tmp_prefix, prefix_selection):
    if prefix_selection == "prefix":
        infos = helpers.info("-p", tmp_prefix)
    elif prefix_selection == "name":
        infos = helpers.info("-n", tmp_env_name)
    else:
        infos = helpers.info()

    assert f"envs directories : {tmp_root_prefix / 'envs'}" in infos
    assert f"environment : {tmp_env_name} (active)" in infos
    assert f"env location : {tmp_prefix}" in infos
    assert f"user config files : {tmp_home / '.mambarc'}" in infos
    assert f"base environment : {tmp_root_prefix}" in infos


@pytest.mark.parametrize("existing_prefix", [False, True])
@pytest.mark.parametrize("prefix_selection", [None, "env_var", "prefix", "name"])
def test_not_env(tmp_home, tmp_root_prefix, prefix_selection, existing_prefix):
    name = "not_an_env"
    prefix = tmp_root_prefix / "envs" / name

    if existing_prefix:
        prefix.mkdir(parents=True, exist_ok=False)

    if prefix_selection == "prefix":
        infos = helpers.info("-p", prefix)
    elif prefix_selection == "name":
        infos = helpers.info("-n", name)
    elif prefix_selection == "env_var":
        os.environ["CONDA_PREFIX"] = str(prefix)
        infos = helpers.info()
    else:
        os.environ.pop("CONDA_PREFIX", "")
        infos = helpers.info()

    if prefix_selection is None:
        # Fallback on root prefix
        expected_name = "base"
        location = tmp_root_prefix
    elif prefix_selection == "env_var":
        expected_name = name + " (active)"
        location = prefix
    else:
        if existing_prefix:
            expected_name = name + " (not env)"
        else:
            expected_name = name + " (not found)"
        location = prefix

    assert f"envs directories : {tmp_root_prefix / 'envs'}" in infos
    assert f"environment : {expected_name}" in infos
    assert f"env location : {location}" in infos
    assert f"user config files : {tmp_home / '.mambarc'}" in infos
    assert f"base environment : {tmp_root_prefix}" in infos


def flags_test(tmp_root_prefix, infos, base_flag, envs_flag, json_flag):
    items = [
        "libmamba version",
        "mamba version",
        "curl version",
        "libarchive version",
        "envs directories",
        "package cache",
        "environment",
        "env location",
        "user config files",
        "populated config files",
        "user config files",
        "virtual packages",
        "channels",
        "platform",
    ]
    if base_flag == "--base":
        if json_flag == "--json":
            assert all(i not in infos.keys() for i in items)
            base_environment_path = infos["base environment"].strip()
        else:
            assert all(
                (f"{i} :" not in infos) | (f"\n{i} :" not in infos) for i in items
            )  # f"\n{i} :" is there to handle the case of the "environment" item (which appears in "base environment" as well)
            base_environment_path = infos.replace("base environment :", "").strip()
        assert os.path.exists(base_environment_path)
        assert base_environment_path == str(tmp_root_prefix)
    elif envs_flag in ["-e", "--envs"]:
        if json_flag == "--json":
            assert str(tmp_root_prefix) in infos["envs"]
        else:
            assert "Name" in infos
            assert "base" in infos
            assert str(tmp_root_prefix) in infos
            all_lines = infos.splitlines()
            for line in all_lines:
                if "*" in line:
                    active_env_l = line
            assert str(tmp_root_prefix) in active_env_l
    else:
        items += ["base environment"]
        if json_flag == "--json":
            assert all(i in infos.keys() for i in items)
        else:
            assert all(f"{i} :" in infos for i in items)


@pytest.mark.parametrize("base_flag", ["", "--base"])
@pytest.mark.parametrize("envs_flag", ["", "-e", "--envs"])
@pytest.mark.parametrize("json_flag", ["", "--json"])
@pytest.mark.parametrize("prefix_selection", [None, "prefix", "name"])
def test_base_flags(
    tmp_home, tmp_root_prefix, prefix_selection, base_flag, envs_flag, json_flag, monkeypatch
):
    monkeypatch.setenv("CONDA_PREFIX", str(tmp_root_prefix))

    if prefix_selection == "prefix":
        infos = helpers.info("-p", tmp_root_prefix, base_flag, envs_flag, json_flag)
    elif prefix_selection == "name":
        infos = helpers.info("-n", "base", base_flag, envs_flag, json_flag)
    else:
        infos = helpers.info(base_flag, envs_flag, json_flag)

    flags_test(tmp_root_prefix, infos, base_flag, envs_flag, json_flag)


@pytest.mark.parametrize("base_flag", ["", "--base"])
@pytest.mark.parametrize("envs_flag", ["", "-e", "--envs"])
@pytest.mark.parametrize("json_flag", ["", "--json"])
@pytest.mark.parametrize("prefix_selection", [None, "prefix", "name"])
def test_env_flags(
    tmp_home,
    tmp_root_prefix,
    tmp_env_name,
    tmp_prefix,
    prefix_selection,
    base_flag,
    envs_flag,
    json_flag,
):
    if prefix_selection == "prefix":
        infos = helpers.info("-p", tmp_prefix, base_flag, envs_flag, json_flag)
    elif prefix_selection == "name":
        infos = helpers.info("-n", tmp_env_name, base_flag, envs_flag, json_flag)
    else:
        infos = helpers.info(base_flag, envs_flag, json_flag)

    flags_test(tmp_root_prefix, infos, base_flag, envs_flag, json_flag)
