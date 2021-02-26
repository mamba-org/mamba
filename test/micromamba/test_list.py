import json
import os
import re
import subprocess

import pytest


def umamba_list(*args):
    cwd = os.getcwd()
    umamba = os.path.join(cwd, "build", "micromamba")
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
        filtered_res = umamba_list("python", "--json", quiet_flag)
        res = umamba_list("--json")

        regex = re.compile("python")
        expected = {pkg["name"] for pkg in res if regex.match(pkg["name"])}
        assert expected == {pkg["name"] for pkg in filtered_res}
        assert len(expected) == 2
