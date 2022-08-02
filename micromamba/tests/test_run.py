import os
from pathlib import Path
import random
import shutil
import string
import subprocess
from sys import platform
import time
from tempfile import TemporaryDirectory

import pytest

from .helpers import create, random_string, umamba_run, get_umamba

common_simple_flags = ["", "-d", "--detach", "--clean-env"]
possible_characters_for_process_names = (
    "-_" + string.ascii_uppercase + string.digits + string.ascii_lowercase
)


def generate_label_flags():
    random_string = "".join(
        random.choice(possible_characters_for_process_names) for _ in range(16)
    )
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
        fails = True
        try:
            umamba_run(option_flag, *make_label_flags())
            fails = False
        except:
            fails = True

        assert fails == True

    @pytest.mark.parametrize("option_flag", common_simple_flags)
    @pytest.mark.parametrize("make_label_flags", next_label_flags)
    def test_unknown_exe_fails(self, option_flag, make_label_flags):

        fails = True
        try:
            umamba_run(option_flag, *make_label_flags(), "exe-that-does-not-exists")
            fails = False
        except:
            fails = True

        # In detach mode we fork micromamba and don't have a way to know if the executable exists.
        if option_flag == "-d" or option_flag == "--detach":
            assert fails == False
            return

        assert fails == True

    @pytest.mark.parametrize("option_flag", common_simple_flags)
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

    @pytest.mark.skipif(platform == "win32", reason="win32 does not support lockfiles")
    def test_lock_no_lock(self):
        tmp_env = os.environ.copy()
        umamba = get_umamba()
        run_cmd = [umamba, "run", "sleep", "1"]
        for expected_num_lockfiles, disable_lockfile in [(1, False), (0, True)]:
            with TemporaryDirectory() as tmpdir:
                if disable_lockfile:
                    mambarc = Path(tmpdir) / ".mambarc"
                    with open(mambarc, "w") as fh:
                        fh.write("disable_lockfile: true")
                files = list((Path(tmpdir)/".mamba").rglob("*"))
                assert len(files) == 0
                tmp_env["HOME"] = tmpdir
                with subprocess.Popen(run_cmd, env=tmp_env) as proc:
                    # give enough time for lockfiles to be created
                    time.sleep(0.1)
                    poll = proc.poll()
                    assert poll is None, f"micromamba run failed {' '.join(run_cmd)}"
                    files = [x for x in (Path(tmpdir)/".mamba").rglob("*") if x.suffix == ".json"]
                    assert len(files) == expected_num_lockfiles
                    proc.kill()
                    assert poll is None, f"micromamba run failed {' '.join(run_cmd)}"

    @pytest.mark.skipif(platform == "win32", reason="requires bash to be available")
    def test_shell_io_routing(self):
        test_script_file_name = "test_run.sh"
        test_script_path = os.path.join(
            os.path.dirname(__file__), test_script_file_name
        )
        if not os.path.isfile(test_script_path):
            raise RuntimeError(
                "missing test script '{}' at '{}".format(
                    test_script_file_name, test_script_path
                )
            )
        assert subprocess.run(test_script_path, shell=True).returncode == 0


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
        res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import sys; print(sys.prefix)"
        )
        assert res.strip() == temp_env_prefix
