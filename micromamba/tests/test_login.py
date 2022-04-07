import base64
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
from xprocess import ProcessStarter

from .helpers import create as umamba_create
from .helpers import get_env, get_umamba, login, logout, random_string

here = Path(__file__).absolute()
pyserver = here.parent.parent.parent / "mamba" / "tests" / "reposerver.py"
channel_directory = here.parent.parent.parent / "mamba" / "tests" / "channel_a"
print(pyserver)

assert pyserver.exists()

auth_file = Path.home() / ".mamba" / "auth" / "authentication.json"


def reposerver(
    xprocess,
    auth,
    port=1234,
    token=None,
    user=None,
    password=None,
    channel_directory=channel_directory,
):
    computed_args = [sys.executable, "-u", pyserver, "--auth", auth, "--port", port]
    computed_args += ["--directory", channel_directory]
    if token:
        computed_args += ["--token", token]
    elif user and password:
        computed_args += ["--user", user, "--password", password]

    class Starter(ProcessStarter):
        # startup pattern
        pattern = "Server started at localhost:"

        # command to start process
        args = computed_args

    # ensure process is running and return its logfile
    logfile = xprocess.ensure("reposerver", Starter)

    conn = (
        f"http://localhost:{port}"  # create a connection or url/port info to the server
    )

    yield conn

    # clean up whole process tree afterwards
    xprocess.getinfo("reposerver").terminate()


@pytest.fixture
def token_server(token, xprocess):
    yield from reposerver(xprocess, "token", token=token)


@pytest.fixture
def basic_auth_server(user, password, xprocess):
    yield from reposerver(xprocess, "basic", user=user, password=password)


def create(*in_args, folder=None, root=None, override_channels=True):
    args = [arg for arg in in_args]

    if folder:
        args += ["-p", str(folder)]
    else:
        args += ["--dry-run", "-n", random_string()]

    if root:
        args += ["--root-prefix", str(root)]

    if override_channels:
        args += ["--override-channels"]

    return umamba_create(
        # "--json",
        *args,
        default_channel=False,
    )


def test_login_logout():
    login("https://myserver.com:1234", "--token", "mytoken")
    login("https://myserver2.com:1234", "--token", "othertoken")

    with open(auth_file) as fi:
        data = json.load(fi)
    assert data["https://myserver.com:1234"]["token"] == "mytoken"
    assert data["https://myserver2.com:1234"]["token"] == "othertoken"

    logout("https://myserver.com:1234")
    with open(auth_file) as fi:
        data = json.load(fi)

    assert "https://myserver.com:1234" not in data


@pytest.mark.parametrize("token", ["crazytoken1234"])
def test_token(token, token_server):
    login(token_server, "--token", token)
    with open(auth_file) as fi:
        data = json.load(fi)
    assert token_server in data
    assert data[token_server]["token"] == token

    res = create("-c", token_server, "testpkg", "--json")
    pkg = res["actions"]["FETCH"][0]
    assert pkg["name"] == "testpkg"


@pytest.mark.parametrize("user,password", [["testuser", "xyzpass"]])
def test_basic_auth(user, password, basic_auth_server):
    login(basic_auth_server, "--username", user, "--password", password)
    with open(auth_file) as fi:
        data = json.load(fi)
    assert basic_auth_server in data
    assert data[basic_auth_server]["password"] == base64.b64encode(
        password.encode("utf-8")
    ).decode("utf-8")

    res = create("-c", basic_auth_server, "testpkg", "--json")
    pkg = res["actions"]["FETCH"][0]
    assert pkg["name"] == "testpkg"


env_yaml_content = """
name: example_env
channels:
- {server}
dependencies:
- _r-mutex
"""

env_file_content = """
# This file may be used to create an environment using:
# $ conda create --name <env> --file <this file>
# platform: linux-64
# env_hash: 8ef82ce4a41fff8289b0b0fbc60703426eb2144d9bcd6fcf481c3e67d4da070f
@EXPLICIT
{server}/noarch/_r-mutex-1.0.1-anacondar_1.tar.bz2#19f9db5f4f1b7f5ef5f6d67207f25f38
"""


@pytest.mark.parametrize("user,password", [["testuser", "xyzpass"]])
def test_basic_auth_explicit(user, password, basic_auth_server, tmp_path):
    login(basic_auth_server, "--username", user, "--password", password)

    env_file = tmp_path / "environment.txt"
    env_file.write_text(env_file_content.format(server=basic_auth_server))
    env_folder = tmp_path / "env"
    root_folder = tmp_path / "root"
    res = create("-f", str(env_file), folder=env_folder, root=root_folder)

    assert (env_folder / "conda-meta" / "_r-mutex-1.0.1-anacondar_1.json").exists()


@pytest.mark.parametrize("user,password", [["testuser", "xyzpass"]])
def test_basic_auth_explicit(user, password, basic_auth_server, tmp_path):
    login(basic_auth_server, "--username", user, "--password", password)

    env_file = tmp_path / "environment.yml"
    env_file.write_text(env_yaml_content.format(server=basic_auth_server))
    env_folder = tmp_path / "env"
    root_folder = tmp_path / "root"
    res = create(
        "-f",
        str(env_file),
        folder=env_folder,
        root=root_folder,
        override_channels=False,
    )

    assert (env_folder / "conda-meta" / "_r-mutex-1.0.1-anacondar_1.json").exists()


@pytest.mark.parametrize("token", ["randomverystrongtoken"])
def test_token_explicit(token, token_server, tmp_path):
    login(token_server, "--token", token)

    env_file = tmp_path / "environment.txt"
    env_file.write_text(env_file_content.format(server=token_server))
    env_folder = tmp_path / "env"
    root_folder = tmp_path / "root"
    res = create("-f", str(env_file), folder=env_folder, root=root_folder)

    assert (env_folder / "conda-meta" / "_r-mutex-1.0.1-anacondar_1.json").exists()
