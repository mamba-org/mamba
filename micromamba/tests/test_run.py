import json
import os
import random
import shutil
import string
import subprocess
from sys import platform

import pytest

from .helpers import create, random_string, subprocess_run, umamba_run

common_simple_flags = ["", "-d", "--detach", "--clean-env"]
# -d/--detach are not available on Windows (see run.cpp)
common_simple_flags_for_help = (
    [f for f in common_simple_flags if f not in ("-d", "--detach")]
    if platform == "win32"
    else common_simple_flags
)
possible_characters_for_process_names = (
    "-_" + string.ascii_uppercase + string.digits + string.ascii_lowercase
)


def generate_label_flags():
    random_string = "".join(random.choice(possible_characters_for_process_names) for _ in range(16))
    return ["--label", random_string]


next_label_flags = [lambda: [], generate_label_flags] if platform != "win32" else []


def simple_short_program():
    return "ls" if platform != "win32" else "dir"


class TestRun:
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    @pytest.mark.parametrize("option_flag", common_simple_flags)
    @pytest.mark.parametrize("make_label_flags", next_label_flags)
    def test_fail_without_command(self, option_flag, make_label_flags):
        with pytest.raises(subprocess.CalledProcessError):
            umamba_run(option_flag, *make_label_flags())

    @pytest.mark.parametrize("option_flag", common_simple_flags)
    @pytest.mark.parametrize("make_label_flags", next_label_flags)
    def test_unknown_exe_fails(self, option_flag, make_label_flags):
        fails = True
        try:
            umamba_run(option_flag, *make_label_flags(), "exe-that-does-not-exists")
            fails = False
        except subprocess.CalledProcessError:
            fails = True

        # In detach mode we fork micromamba and don't have a way to know if the executable exists.
        if option_flag == "-d" or option_flag == "--detach":
            assert fails is False
        else:
            assert fails is True

    @pytest.mark.parametrize("option_flag", common_simple_flags_for_help)
    # @pytest.mark.parametrize("label_flags", naming_flags()) # TODO: reactivate after fixing help flag not disactivating the run
    @pytest.mark.parametrize("help_flag", ["-h", "--help"])
    @pytest.mark.parametrize("command", ["", simple_short_program()])
    def test_help_succeeds(self, option_flag, help_flag, command):
        res = umamba_run(option_flag, help_flag, command)
        assert len(res) > 0

    @pytest.mark.parametrize("option_flag", common_simple_flags)
    @pytest.mark.parametrize("make_label_flags", next_label_flags)
    def test_basic_succeeds(self, option_flag, make_label_flags):
        res = umamba_run(option_flag, *make_label_flags(), simple_short_program())
        print(res)
        assert len(res) > 0

    @pytest.mark.skipif(platform != "linux", reason="uses XDG_CACHE_HOME")
    @pytest.mark.parametrize("record", ["", '{"name":'])
    @pytest.mark.parametrize("label_flags", [[], ["--label", "new-process"]])
    def test_run_ignores_malformed_process_record(self, tmp_path, monkeypatch, record, label_flags):
        monkeypatch.setenv("XDG_CACHE_HOME", str(tmp_path))
        proc_dir = tmp_path / "mamba" / "proc"
        proc_dir.mkdir(parents=True)
        malformed_record = proc_dir / "999999.json"
        malformed_record.write_text(record)
        valid_record = proc_dir / f"{os.getpid()}.json"
        valid_record.write_text(
            json.dumps({"name": "existing-process", "command": ["true"], "prefix": self.current_prefix})
        )

        assert umamba_run(*label_flags, "true") == ""
        assert malformed_record.read_text() == record
        assert json.loads(valid_record.read_text())["name"] == "existing-process"

        with pytest.raises(subprocess.CalledProcessError) as exc:
            umamba_run("--label", "existing-process", "true")
        assert (
            "Another process with name 'existing-process' is currently running."
            in exc.value.stderr.decode()
        )

    @pytest.mark.skipif(platform == "win32", reason="bash specific test")
    @pytest.mark.parametrize("inp", ["(", "a\nb", "a'b\""])
    def test_quoting(self, inp):
        res = umamba_run("echo", inp)
        assert res.strip() == inp

    @pytest.mark.skipif(platform == "win32", reason="requires bash to be available")
    def test_shell_io_routing(self):
        test_script_file_name = "test_run.sh"
        test_script_path = os.path.join(os.path.dirname(__file__), test_script_file_name)
        if not os.path.isfile(test_script_path):
            raise RuntimeError(
                f"missing test script '{test_script_file_name}' at '{test_script_path}"
            )
        subprocess_run(test_script_path, shell=True)

    def test_run_non_existing_env(self):
        env_name = random_string()
        try:
            umamba_run("-n", env_name, "python")
        except subprocess.CalledProcessError as e:
            assert "critical libmamba The given prefix does not exist:" in e.stderr.decode()

    def test_run_non_existing_cwd(self):
        cwd = random_string()
        try:
            umamba_run("--cwd", cwd, "python")
        except subprocess.CalledProcessError as e:
            assert "critical libmamba The given path does not exist:" in e.stderr.decode()


@pytest.fixture()
def temp_env_prefix():
    previous_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    previous_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    os.environ["MAMBA_ROOT_PREFIX"] = root_prefix
    create("-p", prefix, "python")

    yield prefix

    shutil.rmtree(prefix)
    os.environ["MAMBA_ROOT_PREFIX"] = previous_root_prefix
    os.environ["CONDA_PREFIX"] = previous_prefix


class TestRunVenv:
    def test_classic_specs(self, temp_env_prefix):
        res = umamba_run("-p", temp_env_prefix, "python", "-c", "import sys; print(sys.prefix)")
        assert res.strip() == temp_env_prefix
