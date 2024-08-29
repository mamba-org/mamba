import os
import sys
import platform
import subprocess
import time
from pathlib import Path

import pytest

# Need to import everything to get fixtures
from .helpers import *  # noqa: F403
from . import helpers

__this_dir__ = Path(__file__).parent.resolve()


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.parametrize("env_selector", ["", "name", "prefix"])
def test_remove(tmp_home, tmp_root_prefix, env_selector, tmp_xtensor_env, tmp_env_name):
    env_pkgs = [p["name"] for p in helpers.umamba_list("-p", tmp_xtensor_env, "--json")]

    if env_selector == "prefix":
        res = helpers.remove("xtensor", "-p", tmp_xtensor_env, "--json")
    elif env_selector == "name":
        res = helpers.remove("xtensor", "-n", tmp_env_name, "--json")
    else:
        res = helpers.remove("xtensor", "--dry-run", "--json")

    keys = {"dry_run", "success", "prefix", "actions"}
    assert keys.issubset(set(res.keys()))
    assert res["success"]
    assert len(res["actions"]["UNLINK"]) == len(env_pkgs)
    for p in res["actions"]["UNLINK"]:
        assert p["name"] in env_pkgs
    assert res["actions"]["PREFIX"] == str(tmp_xtensor_env)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
@pytest.mark.skipif(sys.platform == "win32", reason="This test is currently failing on Windows")
def test_remove_orphaned(tmp_home, tmp_root_prefix, tmp_xtensor_env, tmp_env_name):
    env_pkgs = [p["name"] for p in helpers.umamba_list("-p", tmp_xtensor_env, "--json")]
    helpers.install("xtensor-python", "-n", tmp_env_name, no_dry_run=True)

    res = helpers.remove("xtensor-python", "-p", tmp_xtensor_env, "--json")

    keys = {"dry_run", "success", "prefix", "actions"}
    assert keys.issubset(set(res.keys()))
    assert res["success"]

    assert len(res["actions"]["UNLINK"]) > 1
    assert res["actions"]["UNLINK"][0]["name"] == "xtensor-python"
    assert res["actions"]["PREFIX"] == str(tmp_xtensor_env)

    res = helpers.remove("xtensor", "-p", tmp_xtensor_env, "--json")

    keys = {"dry_run", "success", "prefix", "actions"}
    assert keys.issubset(set(res.keys()))
    assert res["success"]
    # TODO: find a better use case so we can revert to len(env_pkgs) instead
    # of magic number
    # assert len(res["actions"]["UNLINK"]) == len(env_pkgs) + (
    assert len(res["actions"]["UNLINK"]) == 3 + (
        1 if helpers.dry_run_tests == helpers.DryRun.DRY else 0
    ) + (platform.system() == "Linux")  # xtl is not removed on Linux
    for p in res["actions"]["UNLINK"]:
        assert p["name"] in env_pkgs
    assert res["actions"]["PREFIX"] == str(tmp_xtensor_env)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_remove_force(tmp_home, tmp_root_prefix, tmp_xtensor_env, tmp_env_name):
    # check that we can remove a package without solving the environment (putting
    # it in a bad state, actually)
    helpers.install("xtensor-python", "-n", tmp_env_name, no_dry_run=True)

    res = helpers.remove("xtl", "-p", str(tmp_xtensor_env), "--json", "--force")

    keys = {"dry_run", "success", "prefix", "actions"}
    assert keys.issubset(set(res.keys()))
    assert res["success"]
    assert len(res["actions"]["UNLINK"]) == 1
    assert res["actions"]["UNLINK"][0]["name"] == "xtl"
    assert res["actions"]["PREFIX"] == str(tmp_xtensor_env)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_remove_no_prune_deps(tmp_home, tmp_root_prefix, tmp_xtensor_env, tmp_env_name):
    helpers.install("xtensor-python", "-n", tmp_env_name, no_dry_run=True)

    res = helpers.remove("xtensor", "-p", tmp_xtensor_env, "--json", "--no-prune-deps")

    keys = {"dry_run", "success", "prefix", "actions"}
    assert keys.issubset(set(res.keys()))
    assert res["success"]
    assert len(res["actions"]["UNLINK"]) == 2
    removed_names = [x["name"] for x in res["actions"]["UNLINK"]]
    assert "xtensor" in removed_names
    assert "xtensor-python" in removed_names
    assert res["actions"]["PREFIX"] == str(tmp_xtensor_env)


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_remove_in_use(tmp_home, tmp_root_prefix, tmp_xtensor_env, tmp_env_name):
    helpers.install("python=3.9", "-n", tmp_env_name, "--json", no_dry_run=True)
    if platform.system() == "Windows":
        pyexe = Path(tmp_xtensor_env) / "python.exe"
    else:
        pyexe = Path(tmp_xtensor_env) / "bin" / "python"

    env = helpers.get_fake_activate(tmp_xtensor_env)

    pyproc = subprocess.Popen(
        pyexe, stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env
    )
    time.sleep(1)

    helpers.remove("python", "-v", "-p", str(tmp_xtensor_env), no_dry_run=True)

    if platform.system() == "Windows":
        pyexe_trash = Path(str(pyexe) + ".mamba_trash")
        assert pyexe.exists() is False
        pyexe_trash_exists = pyexe_trash.exists()
        trash_file = Path(tmp_xtensor_env) / "conda-meta" / "mamba_trash.txt"

        if pyexe_trash_exists:
            assert pyexe_trash.exists()
            assert trash_file.exists()
            all_trash_files = list(Path(tmp_xtensor_env).rglob("*.mamba_trash"))

            with open(trash_file, "r") as fi:
                lines = [x.strip() for x in fi.readlines()]
                assert all([line.endswith(".mamba_trash") for line in lines])
                assert len(all_trash_files) == len(lines)
                linesp = [Path(tmp_xtensor_env) / line for line in lines]
                for atf in all_trash_files:
                    assert atf in linesp
        else:
            assert trash_file.exists() is False
            assert pyexe_trash.exists() is False
        # No change if file still in use
        helpers.install("cpp-filesystem", "-n", tmp_env_name, "--json", no_dry_run=True)

        if pyexe_trash_exists:
            assert trash_file.exists()
            assert pyexe_trash.exists()

            with open(trash_file, "r") as fi:
                lines = [x.strip() for x in fi.readlines()]
                assert all([line.endswith(".mamba_trash") for line in lines])
                assert len(all_trash_files) == len(lines)
                linesp = [Path(tmp_xtensor_env) / line for line in lines]
                for atf in all_trash_files:
                    assert atf in linesp
        else:
            assert trash_file.exists() is False
            assert pyexe_trash.exists() is False

        subprocess.Popen("TASKKILL /F /PID {pid} /T".format(pid=pyproc.pid))
        # check that another env mod clears lingering trash files
        time.sleep(0.5)
        helpers.install("xsimd", "-n", tmp_env_name, "--json", no_dry_run=True)
        assert trash_file.exists() is False
        assert pyexe_trash.exists() is False

    else:
        assert pyexe.exists() is False
        pyproc.kill()


@pytest.mark.parametrize("shared_pkgs_dirs", [True], indirect=True)
def test_remove_then_clean(tmp_home, tmp_root_prefix):
    env_file = __this_dir__ / "env-requires-pip-install.yaml"
    env_name = "env_to_clean"
    helpers.create("-n", env_name, "-f", env_file, no_dry_run=True)
    helpers.remove("-n", env_name, "pip", no_dry_run=True)
    helpers.clean("-ay", no_dry_run=True)


def remove_config_common_assertions(res, root_prefix, target_prefix):
    assert res["root_prefix"] == str(root_prefix)
    assert res["target_prefix"] == str(target_prefix)
    assert res["use_target_prefix_fallback"]
    checks = (
        helpers.MAMBA_ALLOW_EXISTING_PREFIX
        | helpers.MAMBA_NOT_ALLOW_MISSING_PREFIX
        | helpers.MAMBA_NOT_ALLOW_NOT_ENV_PREFIX
        | helpers.MAMBA_EXPECT_EXISTING_PREFIX
    )
    assert res["target_prefix_checks"] == checks


def test_remove_config_specs(tmp_home, tmp_root_prefix, tmp_prefix):
    specs = ["xtensor-python", "xtl"]
    cmd = list(specs)

    res = helpers.remove(*cmd, "--print-config-only")

    remove_config_common_assertions(res, root_prefix=tmp_root_prefix, target_prefix=tmp_prefix)
    assert res["env_name"] == ""
    assert res["specs"] == specs


@pytest.mark.parametrize("use_root_prefix", (None, "env_var", "cli"))
@pytest.mark.parametrize("target_is_root", (False, True))
@pytest.mark.parametrize("cli_prefix", (False, True))
@pytest.mark.parametrize("cli_env_name", (False, True))
@pytest.mark.parametrize("env_var", (False, True))
@pytest.mark.parametrize("fallback", (False, True))
def test_remove_config_target_prefix(
    tmp_home,
    tmp_root_prefix,
    tmp_env_name,
    tmp_prefix,
    use_root_prefix,
    target_is_root,
    cli_prefix,
    cli_env_name,
    env_var,
    fallback,
):
    (tmp_root_prefix / "conda-meta").mkdir(parents=True, exist_ok=True)

    cmd = []

    if use_root_prefix in (None, "cli"):
        os.environ["MAMBA_DEFAULT_ROOT_PREFIX"] = os.environ.pop("MAMBA_ROOT_PREFIX")

    if use_root_prefix == "cli":
        cmd += ["-r", str(tmp_root_prefix)]

    r = str(tmp_root_prefix)

    if target_is_root:
        p = r
        n = "base"
    else:
        p = str(tmp_prefix)
        n = str(tmp_env_name)

    if cli_prefix:
        cmd += ["-p", p]

    if cli_env_name:
        cmd += ["-n", n]

    if env_var:
        os.environ["MAMBA_TARGET_PREFIX"] = p

    if not fallback:
        os.environ.pop("CONDA_PREFIX")
    else:
        os.environ["CONDA_PREFIX"] = p

    if (cli_prefix and cli_env_name) or not (cli_prefix or cli_env_name or env_var or fallback):
        with pytest.raises(subprocess.CalledProcessError):
            helpers.remove(*cmd, "--print-config-only")
    else:
        res = helpers.remove(*cmd, "--print-config-only")
        remove_config_common_assertions(res, root_prefix=r, target_prefix=p)
