import json
import os
import platform
import shutil
import subprocess
from pathlib import Path, PureWindowsPath

import pytest

from . import helpers


def skip_if_shell_incompat(shell_type):
    """Skip test if ``shell_type`` is incompatible with the platform"""
    plat_system = platform.system()
    if (
        (plat_system == "Linux" and shell_type not in ("bash", "posix", "dash"))
        or (plat_system == "Windows" and shell_type not in ("cmd.exe", "powershell"))
        or (plat_system == "Darwin" and shell_type not in ("zsh", "bash", "posix", "dash"))
    ):
        pytest.skip("Incompatible shell/OS")


@pytest.mark.parametrize(
    "shell_type",
    ["bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh", "fish", "tcsh", "nu"],
)
def test_hook(tmp_home, tmp_root_prefix, shell_type):
    res = helpers.shell("hook", "-s", shell_type)

    mamba_exe = helpers.get_umamba()
    mamba_exe_posix = PureWindowsPath(mamba_exe).as_posix()
    # suspend long path support on Windows
    # if platform.system() == "Windows":
    # mamba_exe = f"\\\\?\\{mamba_exe}"

    if shell_type == "powershell":
        assert f"$Env:MAMBA_EXE='{mamba_exe}'" in res
        lines = res.splitlines()
        assert not any(li.startswith("param([") for li in lines)
        assert not any(li.startswith("## EXPORTS ##") for li in lines)
        assert lines[2].startswith("## AFTER PARAM ####")
    elif shell_type in ("zsh", "bash", "posix"):
        assert res.count(mamba_exe_posix) == 5
    elif shell_type == "xonsh":
        assert res.count(mamba_exe_posix) == 8
    elif shell_type == "fish":
        assert res.count(mamba_exe_posix) == 5
    elif shell_type == "cmd.exe":
        assert res == ""
    elif shell_type == "tcsh":
        assert res.count(mamba_exe_posix) == 5
    elif shell_type == "nu":
        # insert dummy test, as the nu scripts contains
        # no mention of mamba_exe; path is added in config.nu
        assert res.count(mamba_exe_posix) == 0

    res = helpers.shell("hook", "-s", shell_type, "--json")
    expected_keys = {"success", "operation", "context", "actions"}
    assert set(res.keys()) == expected_keys

    assert res["success"]
    assert res["operation"] == "shell_hook"
    assert res["context"]["shell_type"] == shell_type
    assert set(res["actions"].keys()) == {"print"}

    if shell_type != "cmd.exe":
        assert res["actions"]["print"]


def test_auto_detection(tmp_home, tmp_root_prefix):
    def decode_json_output(res):
        try:
            j = json.loads(res)
            return j
        except json.decoder.JSONDecodeError as e:
            print(f"Error when loading JSON output from {res}")
            raise (e)

    def custom_shell(shell):
        umamba = helpers.get_umamba()
        f_name = tmp_root_prefix / "shell_script"
        if shell == "cmd.exe":
            f_name += ".bat"
            cmd = [shell, "/c", f_name]
            with open(f_name, "w") as f:
                f.write(f"@Echo off\r\n{umamba} shell hook --json")
        elif shell == "powershell":
            f_name += ".ps1"
            cmd = [shell, f_name]
            with open(f_name, "w", encoding="utf-8") as f:
                f.write(f"& {umamba} shell hook --json")
        else:
            cmd = [shell, f_name]
            with open(f_name, "w", encoding="utf-8") as f:
                f.write(f"{umamba} shell hook --json")
        res = subprocess.run(cmd, text=True, encoding="utf-8")
        try:
            print(res.stdout)
            print(res.stderr)
        except Exception:
            pass
        return decode_json_output(subprocess.check_output(cmd, text=True, encoding="utf-8"))

    if platform.system() == "Windows":
        if "MAMBA_TEST_SHELL_TYPE" not in os.environ:
            pytest.skip("'MAMBA_TEST_SHELL_TYPE' env variable needs to be defined to run this test")
        shell_type = os.environ["MAMBA_TEST_SHELL_TYPE"]
        if shell_type == "bash":
            pytest.skip(
                "Currently not working because Github Actions complains about bash"
                " not being available from WSL"
            )
    elif platform.system() in ("Linux", "Darwin"):
        shell_type = "bash"
    else:
        pytest.skip("Unsupported platform")

    res = custom_shell(shell_type)

    expected_keys = {"success", "operation", "context", "actions"}
    assert set(res.keys()) == expected_keys

    assert res["success"]
    assert res["operation"] == "shell_hook"
    assert res["context"]["shell_type"] == shell_type
    assert set(res["actions"].keys()) == {"print"}
    assert res["actions"]["print"]

    if shell_type != "cmd.exe":
        assert res["actions"]["print"][0]
    else:
        assert res["actions"]["print"][0] == ""


@pytest.mark.parametrize("shell_type", ["bash", "posix", "powershell", "cmd.exe"])
@pytest.mark.parametrize("prefix_is_root", [False, True])
@pytest.mark.parametrize("prefix_exists", [False, True])
@pytest.mark.parametrize("prefix_type", ["shrunk_prefix", "expanded_prefix", "name"])
def test_activate(
    tmp_home,
    tmp_root_prefix,
    tmp_prefix,
    tmp_env_name,
    shell_type,
    prefix_is_root,
    prefix_exists,
    prefix_type,
):
    skip_if_shell_incompat(shell_type)

    if prefix_is_root:
        p = tmp_root_prefix
        n = "base"
    else:
        p = tmp_prefix
        n = tmp_env_name

    if not prefix_exists:
        shutil.rmtree(p)

    cmd = ["activate", "-s", shell_type]
    if prefix_type == "expanded_prefix":
        cmd.append(p)
    elif prefix_type == "shrunk_prefix":
        cmd.append(str(p).replace(os.path.expanduser("~"), "~"))
    else:
        cmd.append(n)

    if prefix_exists:
        res = helpers.shell(*cmd)
    else:
        with pytest.raises(subprocess.CalledProcessError):
            helpers.shell(*cmd)
        return

    # TODO: improve this test
    assert res

    if shell_type == "bash":
        assert f"export CONDA_PREFIX='{p}'" in res
        assert f"export CONDA_DEFAULT_ENV='{n}'" in res
        assert f"export CONDA_PROMPT_MODIFIER='({n}) '" in res


def test_activate_target_prefix_checks(tmp_home, tmp_root_prefix):
    """Shell operations have their own 'shell_prefix' Configurable
    and doesn't use 'target_prefix'."""

    res = helpers.shell("activate", "-p", tmp_root_prefix, "--print-config-only")
    assert res["target_prefix_checks"] == helpers.MAMBA_NO_PREFIX_CHECK
    assert not res["use_target_prefix_fallback"]
    assert not res["use_default_prefix_fallback"]
    assert not res["use_root_prefix_fallback"]


@pytest.mark.parametrize("shell_type", ["bash", "powershell", "cmd.exe"])
@pytest.mark.parametrize("prefix_selector", [None, "prefix"])
@pytest.mark.parametrize("multiple_time,same_prefix", ((False, None), (True, False), (True, True)))
def test_init(tmp_home, tmp_root_prefix, shell_type, prefix_selector, multiple_time, same_prefix):
    skip_if_shell_incompat(shell_type)

    if prefix_selector is None:
        res = helpers.shell("-y", "init", "-s", shell_type)
    else:
        res = helpers.shell("-y", "init", "-s", shell_type, "-r", tmp_root_prefix)
    assert res

    if multiple_time:
        if same_prefix and shell_type == "cmd.exe":
            res = helpers.shell("-y", "init", "-s", shell_type, "-r", tmp_root_prefix)
            lines = res.splitlines()
            assert "cmd.exe already initialized." in lines
            # TODO test deactivated when enabled micromamba as "mamba" executable.
            # The test failed for some reason.
            # We would like a more controlled way to test long path support than into an
            # integration test.
            #  assert "Windows long-path support already enabled." in lines
        else:
            assert helpers.shell("-y", "init", "-s", shell_type, "-r", tmp_root_prefix / "env")

    if shell_type == "bash":
        assert (tmp_root_prefix / "etc" / "profile.d").is_dir()
    else:
        assert (tmp_root_prefix / "condabin").is_dir()


def test_shell_init_with_env_var(tmp_home, tmp_root_prefix):
    umamba_cmd = helpers.get_umamba()
    res = helpers.umamba_run("sh", "-c", f"export SHELL=/bin/bash; {umamba_cmd} shell init")
    assert res
    assert (tmp_root_prefix / "etc" / "profile.d").is_dir()


def test_dash(tmp_home, tmp_root_prefix):
    skip_if_shell_incompat("dash")
    umamba = helpers.get_umamba()
    subprocess.check_call(["dash", "-c", f"eval $({umamba} shell hook -s dash)"])
    subprocess.check_call(["dash", "-c", f"eval $({umamba} shell hook -s posix)"])


def test_implicitly_created_environment(tmp_home, tmp_root_prefix):
    """If a shell implicitly creates an environment, confirm that it's valid.

    See <https://github.com/mamba-org/mamba/pull/2162>.
    """
    skip_if_shell_incompat("bash")
    shutil.rmtree(tmp_root_prefix)
    assert helpers.shell("init", "--shell=bash")
    assert (Path(tmp_root_prefix) / "conda-meta").exists()
    # Check, for example, that "list" works.
    assert helpers.umamba_list("-p", tmp_root_prefix)


def test_shell_run_activated(tmp_home, tmp_prefix):
    """Verify environment properly activated in `micromamba shell`."""
    skip_if_shell_incompat("bash")
    stdout = subprocess.check_output(
        [helpers.get_umamba(), "shell", "-p", tmp_prefix],
        input="echo $PATH",
        text=True,
    )
    assert str(tmp_prefix) in stdout.split(os.pathsep)[0]


@pytest.mark.parametrize("use_prefix", [True, False])
def test_shell_run_SHELL(tmp_home, tmp_prefix, tmp_env_name, use_prefix, tmp_path):
    """ "micromamba shell -n myenv should run $SHELL in myenv."""
    skip_if_shell_incompat("bash")

    script_path = tmp_path / "fakeshell.sh"
    script_path.write_text("#!/bin/sh\nexit 42")
    script_path.chmod(0o777)

    if use_prefix:
        cmd = [helpers.get_umamba(), "shell", "-p", tmp_prefix]
    else:
        cmd = [helpers.get_umamba(), "shell", "-n", tmp_env_name]

    ret = subprocess.run(cmd, env={**os.environ, "SHELL": script_path})
    assert ret.returncode == 42
