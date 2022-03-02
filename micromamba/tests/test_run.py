import os
import random
import shutil
import string
import subprocess
from sys import platform
from warnings import catch_warnings

import pytest

from .helpers import umamba_run, umamba_run_simple

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

    @pytest.mark.skipif(platform == "win32", reason="requires bash to be available")
    def test_bash_output(self):
        assert umamba_run_simple("-n", "base", "/bin/bash", "-c", "test -t 0") == 1
        assert umamba_run_simple("-n", "base", "/bin/bash", "-c", "test -t 1") == 0
        assert umamba_run_simple("-a", "stdin stderr stdout", "-n", "base", "/bin/bash", "-c", "test -t 0") == 1
        assert umamba_run_simple("-a", "stdin stderr stdout", "-n", "base", "/bin/bash", "-c", "test -t 1") == 0
        assert umamba_run_simple("-a", "stderr stdout", "-n", "base", "/bin/bash", "-c", "test -t 0") == 1
        assert umamba_run_simple("-a", "stdin", "-n", "base", "/bin/bash", "-c", "test -t 0") == 1
        assert umamba_run_simple("-a", "", "-n", "base", "/bin/bash", "-c", "test -t 0") == 1
        assert umamba_run_simple("-a", "", "-n", "base", "/bin/bash", "-c", "test -t 1") == 1
        assert umamba_run_simple("-n", "base", "--no-capture-output", "/bin/bash", "-c", "test -t 0") == 0

        
        



