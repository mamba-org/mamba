import json
import os
import platform
import shutil
import subprocess
from pathlib import Path

import pytest

from .helpers import (
    MAMBA_NO_PREFIX_CHECK,
    create,
    get_umamba,
    random_string,
    shell,
    umamba_list,
)


def skip_if_shell_incompat(shell_type):
    """Skip test if ``shell_type`` is incompatible with the platform"""
    plat_system = platform.system()
    if (
        (plat_system == "Linux" and shell_type not in ("bash", "posix", "dash"))
        or (plat_system == "Windows" and shell_type not in ("cmd.exe", "powershell"))
        or (
            plat_system == "Darwin"
            and shell_type not in ("zsh", "bash", "posix", "dash")
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
    def setup_method(cls):
        os.makedirs(TestShell.root_prefix, exist_ok=False)

    @classmethod
    def teardown_method(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestShell.root_prefix
        os.environ["CONDA_PREFIX"] = TestShell.current_prefix

        if Path(TestShell.root_prefix).exists():
            shutil.rmtree(TestShell.root_prefix)

    @pytest.mark.parametrize(
        "shell_type",
        ["bash", "posix", "powershell", "cmd.exe", "xonsh", "zsh", "fish", "tcsh"],
    )
    def test_hook(self, shell_type):
        res = shell("hook", "-s", shell_type)

        mamba_exe = get_umamba()
        # suspend long path support on Windows
        # if platform.system() == "Windows":
        # mamba_exe = f"\\\\?\\{mamba_exe}"

        if shell_type == "powershell":
            assert f"$Env:MAMBA_EXE='{mamba_exe}'" in res
            lines = res.splitlines()
            assert not any(l.startswith("param([") for l in lines)
            assert not any(l.startswith("## EXPORTS ##") for l in lines)
            assert lines[2].startswith("## AFTER PARAM ####")
        elif shell_type in ("zsh", "bash", "posix"):
            assert res.count(mamba_exe) == 3
        elif shell_type == "xonsh":
            assert res.count(mamba_exe) == 8
        elif shell_type == "fish":
            assert res.count(mamba_exe) == 5
        elif shell_type == "cmd.exe":
            assert res == ""
        elif shell_type == "tcsh":
            assert res.count(mamba_exe) == 3

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
            umamba = get_umamba()
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
            except:
                pass
            return decode_json_output(
                subprocess.check_output(cmd, text=True, encoding="utf-8")
            )

        if platform.system() == "Windows":
            if "MAMBA_TEST_SHELL_TYPE" not in os.environ:
                pytest.skip(
                    "'MAMBA_TEST_SHELL_TYPE' env variable needs to be defined to run this test"
                )
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
    @pytest.mark.parametrize(
        "prefix_type", ["shrinked_prefix", "expanded_prefix", "name"]
    )
    def test_activate(self, shell_type, prefix_is_root, prefix_exists, prefix_type):
        skip_if_shell_incompat(shell_type)

        if prefix_exists:
            # Create the environment for this test, so that it exists
            create("-n", TestShell.env_name, "-q", "--offline", no_dry_run=True)
        else:
            if prefix_is_root:
                shutil.rmtree(TestShell.root_prefix)

        if prefix_is_root:
            p = TestShell.root_prefix
            n = "base"
        else:
            p = TestShell.prefix
            n = TestShell.env_name

        cmd = ["activate", "-s", shell_type]
        if prefix_type == "expanded_prefix":
            cmd.append(p)
        elif prefix_type == "shrinked_prefix":
            cmd.append(p.replace(os.path.expanduser("~"), "~"))
        else:
            cmd.append(n)

        if prefix_exists:
            res = shell(*cmd)
        else:
            with pytest.raises(subprocess.CalledProcessError):
                shell(*cmd)
            return

        # TODO: improve this test
        assert res

        if shell_type == "bash":
            assert f"export CONDA_PREFIX='{p}'" in res
            assert f"export CONDA_DEFAULT_ENV='{n}'" in res
            assert f"export CONDA_PROMPT_MODIFIER='({n}) '" in res

    def test_activate_target_prefix_checks(self):
        """Shell operations have their own 'shell_prefix' Configurable
        and doesn't use 'target_prefix'."""

        res = shell("activate", "-p", TestShell.root_prefix, "--print-config-only")
        assert res["target_prefix_checks"] == MAMBA_NO_PREFIX_CHECK
        assert not res["use_target_prefix_fallback"]

    @pytest.mark.parametrize("shell_type", ["bash", "powershell", "cmd.exe"])
    @pytest.mark.parametrize("prefix_selector", [None, "prefix"])
    @pytest.mark.parametrize(
        "multiple_time,same_prefix", ((False, None), (True, False), (True, True))
    )
    def test_init(self, shell_type, prefix_selector, multiple_time, same_prefix):
        skip_if_shell_incompat(shell_type)

        if prefix_selector is None:
            res = shell("-y", "init", "-s", shell_type)
        else:
            res = shell("-y", "init", "-s", shell_type, "-r", TestShell.root_prefix)
        assert res

        if multiple_time:
            if same_prefix and shell_type == "cmd.exe":
                res = shell("-y", "init", "-s", shell_type, "-r", TestShell.root_prefix)
                assert res.splitlines() == [
                    "cmd.exe already initialized.",
                    "Windows long-path support already enabled.",
                ]
            else:
                assert shell(
                    "-y",
                    "init",
                    "-s",
                    shell_type,
                    "-r",
                    os.path.join(TestShell.root_prefix, random_string()),
                )

        if shell_type == "bash":
            assert Path(
                os.path.join(TestShell.root_prefix, "etc", "profile.d")
            ).is_dir()
        else:
            assert Path(os.path.join(TestShell.root_prefix, "condabin")).is_dir()

        shell("init", "-y", "-s", shell_type, "-r", TestShell.current_root_prefix)

    def test_dash(self):
        skip_if_shell_incompat("dash")
        subprocess.check_call(["dash", "-c", "eval $(micromamba shell hook -s dash)"])
        subprocess.check_call(["dash", "-c", "eval $(micromamba shell hook -s posix)"])

    def test_implicitly_created_environment(self):
        """If a shell implicitly creates an environment, confirm that it's valid.

        See <https://github.com/mamba-org/mamba/pull/2162>.
        """
        skip_if_shell_incompat("bash")
        shutil.rmtree(TestShell.root_prefix)
        assert shell("init", "--shell=bash")
        assert (Path(TestShell.root_prefix) / "conda-meta").exists()
        # Check, for example, that "list" works.
        assert umamba_list("-p", TestShell.root_prefix)

    @pytest.mark.parametrize("prefix_selector", [("-p", prefix), ("-n", env_name)])
    def test_shell_run_SHELL(self, prefix_selector, tmp_path):
        """ "micromamba shell -n myenv should run $SHELL in myenv."""
        skip_if_shell_incompat("bash")
        create(*prefix_selector)

        script_path = tmp_path / "fakeshell.sh"
        script_path.write_text("#!/bin/sh\nexit 42")
        script_path.chmod(0o777)

        ret = subprocess.run(
            [get_umamba(), "shell", *prefix_selector],
            env={**os.environ, "SHELL": script_path},
        )
        assert ret.returncode == 42

    def test_shell_run_activated(self):
        """Verify environment properly activated in `micromamba shell`."""
        skip_if_shell_incompat("bash")
        create("-n", self.env_name)
        p = subprocess.Popen(
            [get_umamba(), "shell", "-n", self.env_name],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=subprocess.PIPE,
            text=True,
        )
        stdout, stderr = p.communicate("echo $PATH")
        assert p.poll() == 0  # Terminated with retcode 0
        assert self.env_name in stdout.split(os.pathsep)[0]
