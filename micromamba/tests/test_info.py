import os

import pytest

from . import helpers


@pytest.mark.parametrize("prefix_selection", [None, "prefix", "name"])
def test_base(tmp_home, tmp_root_prefix, prefix_selection):
    os.environ["CONDA_PREFIX"] = str(tmp_root_prefix)

    if prefix_selection == "prefix":
        infos = helpers.info("-p", tmp_root_prefix)
    elif prefix_selection == "name":
        infos = helpers.info("-n", "base")
    else:
        infos = helpers.info()

    assert "environment : base (active)" in infos
    assert f"env location : {tmp_root_prefix}" in infos
    assert f"user config files : {tmp_home / '.mambarc' }" in infos
    assert f"base environment : {tmp_root_prefix}" in infos


@pytest.mark.parametrize("prefix_selection", [None, "prefix", "name"])
def test_env(tmp_home, tmp_root_prefix, tmp_env_name, tmp_prefix, prefix_selection):
    if prefix_selection == "prefix":
        infos = helpers.info("-p", tmp_prefix)
    elif prefix_selection == "name":
        infos = helpers.info("-n", tmp_env_name)
    else:
        infos = helpers.info()

    assert f"environment : {tmp_env_name} (active)" in infos
    assert f"env location : {tmp_prefix}" in infos
    assert f"user config files : {tmp_home / '.mambarc' }" in infos
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
    print(infos)

    assert f"environment : {expected_name}" in infos
    assert f"env location : {location}" in infos
    assert f"user config files : {tmp_home / '.mambarc' }" in infos
    assert f"base environment : {tmp_root_prefix}" in infos
