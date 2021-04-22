import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import pytest

from .helpers import create, get_env, get_umamba, info, random_string, shell


def skip_if_shell_incompat(shell_type):
    """Skip test if ``shell_type`` is incompatible with the platform"""
    if (
        (platform.system() == "Linux" and shell_type not in ("bash", "posix"))
        or (
            platform.system() == "Windows"
            and shell_type not in ("cmd.exe", "powershell")
        )
        or (
            platform.system() == "Darwin"
            and shell_type not in ("zsh", "bash", "posix")
        )
    ):
        pytest.skip("Incompatible shell/OS")


class TestShell:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.root_prefix

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.current_root_prefix

    @classmethod
    def setup(cls):
        os.makedirs(TestShell.root_prefix, exist_ok=True)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.current_root_prefix

    @classmethod
    def teardown(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.root_prefix
        # shutil.rmtree(TestShell.root_prefix)

    @pytest.mark.parametrize(
        "shell_type", ["bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh"]
    )
    def test_hook(self, shell_type):
        res = shell("hook", "-s", shell_type)

        mamba_exe = get_umamba()
        # suspend long path support on Windows
        # if platform.system() == "Windows":
        # mamba_exe = f"\\\\?\\{mamba_exe}"

        if shell_type == "powershell":
            umamba = get_umamba()
            assert f"$Env:MAMBA_EXE='{mamba_exe}'" in res
        elif shell_type in ("zsh", "bash", "posix"):
            assert res.count(mamba_exe) == 5
        elif shell_type == "xonsh":
            assert res.count(mamba_exe) == 8
        elif shell_type == "cmd.exe":
            assert res == ""

        res = shell("hook", "-s", shell_type, "--json")
        expected_keys = {"success", "operation", "context", "actions"}
        assert set(res.keys()) == expected_keys

        assert res["success"]
        assert res["operation"] == "shell_hook"
        assert res["context"]["shell_type"] == shell_type
        assert set(res["actions"].keys()) == {"print"}

        if shell_type != "cmd.exe":
            assert res["actions"]["print"]

    def test_auto_detection(self):
        def decode_json_output(res):
            try:
                j = json.loads(res)
                return j
            except json.decoder.JSONDecodeError as e:
                print(f"Error when loading JSON output from {res}")
                raise (e)

        def custom_shell(shell):
            umamba = get_umamba(cwd=os.getcwd())
            f_name = os.path.join(
                TestShell.root_prefix, "shell_script_" + random_string()
            )
            if shell == "cmd.exe":
                f_name += ".bat"
                cmd = [shell, "/c", f_name]
                with open(f_name, "w") as f:
                    f.write(f"@Echo off\r\n{umamba} shell hook --json")
            elif shell == "powershell":
                f_name += ".ps1"
                cmd = [shell, f_name]
                with open(f_name, "w") as f:
                    if "CI" in os.environ:
                        f.write(
                            f"conda activate {TestShell.current_prefix} | out-null\n"
                        )
                    f.write(f"& {umamba} shell hook --json")
            else:
                cmd = [shell, f_name]
                if platform.system() == "Windows":  # bash on Windows
                    with open(f_name, "w") as f:
                        f.write("build/micromamba.exe shell hook --json")
                else:
                    with open(f_name, "w") as f:
                        f.write(f"{umamba} shell hook --json")

            return decode_json_output(subprocess.check_output(cmd))

        if platform.system() == "Windows":
            if "MAMBA_TEST_SHELL_TYPE" not in os.environ:
                pytest.skip(
                    "'MAMBA_TEST_SHELL_TYPE' env variable needs to be defined to run this test"
                )
            shell_type = os.environ["MAMBA_TEST_SHELL_TYPE"]
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
    @pytest.mark.parametrize("root", [False, True])
    @pytest.mark.parametrize("env_exists", [False, True])
    @pytest.mark.parametrize("expanded_home", [False, True])
    @pytest.mark.parametrize("prefix_type", ["prefix", "name"])
    def test_activate(self, shell_type, root, env_exists, prefix_type, expanded_home):
        skip_if_shell_incompat(shell_type)

        if not root and env_exists:
            create("-n", TestShell.env_name, "-q", "--offline", no_dry_run=True)

        if prefix_type == "prefix":
            if expanded_home:
                cmd = ("activate", "-s", shell_type, "-p", TestShell.prefix)
            else:
                cmd = (
                    "activate",
                    "-s",
                    shell_type,
                    "-p",
                    TestShell.prefix.replace(os.path.expanduser("~"), "~"),
                )
        else:
            cmd = ("activate", "-s", shell_type, "-p", TestShell.env_name)

        res = shell(*cmd)

        # TODO: improve this test
        assert res

        if shell_type == "bash":
            assert f"export CONDA_PREFIX='{TestShell.prefix}'" in res
            assert f"export CONDA_DEFAULT_ENV='{TestShell.env_name}'" in res
            assert f"export CONDA_PROMPT_MODIFIER='({TestShell.env_name}) '" in res

    @pytest.mark.parametrize("shell_type", ["bash", "posix", "powershell", "cmd.exe"])
    def test_activate_non_existent(self, shell_type):
        skip_if_shell_incompat(shell_type)

        cmd = ("activate", "-s", shell_type, "-p", "this-env-does-not-exist")
        with pytest.raises(subprocess.CalledProcessError):
            shell(*cmd)

    @pytest.mark.parametrize("shell_type", ["bash", "powershell", "cmd.exe"])
    @pytest.mark.parametrize("prefix_selector", [None, "prefix"])
    def test_init(self, shell_type, prefix_selector):
        skip_if_shell_incompat(shell_type)

        if prefix_selector:
            shell("-y", "init", "-s", shell_type, "-p", TestShell.root_prefix)
        else:
            with pytest.raises(subprocess.CalledProcessError):
                shell("-y", "init", "-s", shell_type)

        assert (
            Path(os.path.join(TestShell.root_prefix, "condabin")).is_dir()
            or Path(os.path.join(TestShell.root_prefix, "etc", "profile.d")).is_dir()
        )

        shell("init", "-y", "-s", shell_type, "-p", TestShell.current_root_prefix)
