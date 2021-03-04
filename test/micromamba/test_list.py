import json
import os
import re
import subprocess

import pytest

from .helpers import get_umamba


def umamba_list(*args):
    umamba = get_umamba()

    cmd = [umamba, "list"] + [arg for arg in args if arg]
    res = subprocess.check_output(cmd)

    if "--json" in args:
        j = json.loads(res)
        return j

    return res.decode()


class TestList:
    @pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
    def test_without_arguments(self, quiet_flag):
        res = umamba_list("--json", quiet_flag)
        assert len(res) > 0

        sorted_list = sorted(res, key=lambda i: i["name"])
        assert sorted_list == res

    @pytest.mark.parametrize("quiet_flag", ["", "-q", "--quiet"])
    def test_regex(self, quiet_flag):
        search_str = "python"
        filtered_res = umamba_list(search_str, "--json", quiet_flag)
        res = umamba_list("--json")

        expected = {pkg["name"] for pkg in res if search_str in pkg["name"]}
        assert expected == {pkg["name"] for pkg in filtered_res}
