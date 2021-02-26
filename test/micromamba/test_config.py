import json
import os
import subprocess

import pytest


def config(*args):
    cwd = os.getcwd()
    umamba = os.path.join(cwd, "build", "micromamba")
    cmd = [umamba, "config"] + [arg for arg in args if arg]
    res = subprocess.check_output(cmd)
    if "--json" in args:
        j = json.loads(res)
        return j

    return res.decode()


@pytest.fixture(scope="session")
def rc_file(tmpdir_factory):
    fn = tmpdir_factory.mktemp("umamba").join("config.yaml")
    content = "channels:\n  - channel1\n  - channel2\n"
    with open(fn, "w") as f:
        f.write(content)
    return fn


class TestConfig:
    def test_empty(self):
        assert config().startswith("Configuration of micromamba\n")

    @pytest.mark.parametrize("quiet_flag", ["-q", "--quiet"])
    def test_quiet(self, quiet_flag):
        assert config(quiet_flag) == ""


class TestConfigSources:
    @pytest.mark.parametrize("quiet_flag", ["-q", "--quiet"])
    @pytest.mark.parametrize("rc_flag", ["--no-rc", "--rc-file='none'"])
    def test_sources(self, quiet_flag, rc_flag):
        res = config("sources", quiet_flag, rc_flag)
        expected = {
            "--no-rc": "Configuration files disabled by --no-rc flag\n",
            "--rc-file='none'": "Configuration files (by precedence order):\n",
        }
        assert res == expected[rc_flag]


class TestConfigList:
    @pytest.mark.parametrize("rc_flag", ["--no-rc", "--rc-file="])
    def test_list(self, rc_file, rc_flag):
        expected = {
            "--no-rc": "Configuration files disabled by --no-rc flag\n",
            "--rc-file=" + str(rc_file): "channels:\n  - channel1\n  - channel2\n",
        }
        if rc_flag == "--rc-file=":
            rc_flag += str(rc_file)

        assert config("list", rc_flag) == expected[rc_flag]

    def test_list_with_sources(self, rc_file):
        src = f"  # {rc_file}"
        assert (
            config("list", "--rc-file=" + str(rc_file), "--show-source")
            == f"channels:\n  - channel1{src}\n  - channel2{src}\n"
        )
