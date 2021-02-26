import json
import os
import subprocess

import pytest


def install(*args):
    cwd = os.getcwd()
    umamba = os.path.join(cwd, "build", "micromamba")
    cmd = [umamba, "install"] + [arg for arg in args if arg]
    res = subprocess.check_output(cmd)
    if "--json" in args:
        j = json.loads(res)
        return j

    return res.decode()


class TestInstall:
    def test_empty(self):
        assert install() == "Nothing to do.\n"

    def test_already_installed(self):
        res = install("numpy", "--json", "--dry-run")
        keys = {"dry_run", "success", "prefix", "message"}
        assert keys == set(res.keys())
        assert res["success"]

    def test_not_already_installed(self):
        res = install("bokeh", "--json", "--dry-run")
        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys == set(res.keys())
        assert res["success"]

    def test_dependencies(self):
        res = install("xtensor", "--json", "--dry-run")
        keys = {"dry_run", "success", "prefix", "actions"}
        assert keys == set(res.keys())
        assert res["success"]

        action_keys = {"FETCH", "LINK", "PREFIX"}
        assert action_keys == set(res["actions"].keys())

        packages = {pkg["name"] for pkg in res["actions"]["FETCH"]}
        expected_packages = {"xtensor", "xtl"}
        assert packages == expected_packages
