import os
import shutil
import subprocess
from warnings import catch_warnings
from sys import platform

from .helpers import umamba_run

import pytest

common_simple_flags = ["", "-d", "--detach", "--clean-env"]
simple_short_program = "ls" if platform != "win32" else "dir"


class TestRun:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    @pytest.mark.parametrize("option_flag", common_simple_flags)
    def test_fail_without_command(self, option_flag):
        fails = True
        try:
            umamba_run(option_flag)
            fails = False
        except:
            fails = True
        
        assert fails == True

    @pytest.mark.parametrize("option_flag", common_simple_flags)
    def test_unknown_exe_fails(self, option_flag):

        fails = True
        try:
            umamba_run(option_flag, "exe-that-does-not-exists")
            fails = False
        except:
            fails = True
        

        # In detach mode we fork micromamba and don't have a way to know if the executable exists.
        if option_flag == "-d" or option_flag == "--detach":
            assert fails == False
            return

        assert fails == True

    @pytest.mark.parametrize("option_flag", common_simple_flags)
    @pytest.mark.parametrize("help_flag", ["-h", "--help"])
    @pytest.mark.parametrize("command", ["", simple_short_program])
    def test_help_succeeds(self, option_flag, help_flag, command):
        res = umamba_run(option_flag, help_flag, command)
        assert len(res) > 0


    @pytest.mark.parametrize("option_flag", common_simple_flags)
    def test_basic_succeeds(self, option_flag):
        res = umamba_run(option_flag, simple_short_program)
        print(res)
        assert len(res) > 0

    






        




