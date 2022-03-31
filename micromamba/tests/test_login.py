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
from .helpers import get_env, get_umamba, login, random_string

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
        pattern = "Server started at localhost"

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


def create(*args):
    return umamba_create(
        "-n",
        random_string(),
        "--override-channels",
        "--dry-run",
        "--json",
        *args,
        default_channel=False,
    )


@pytest.mark.parametrize("token", ["crazytoken1234"])
def test_token(token, token_server):
    login(token_server, "--token", token)
    with open(auth_file) as fi:
        data = json.load(fi)
    assert token_server in data
    assert data[token_server]["token"] == token

    res = create("-c", token_server, "testpkg")

    pkg = res["actions"]["FETCH"][0]
    assert pkg["name"] == "testpkg"

    print(pkg)

    print(pkg["channel"])
    login(token_server, "--token", token)


@pytest.mark.parametrize("user,password", [["testuser", "xyzpass"]])
def test_basic_auth(user, password, basic_auth_server):
    login(basic_auth_server, "--username", user, "--password", password)
    with open(auth_file) as fi:
        data = json.load(fi)
    assert basic_auth_server in data
    assert data[basic_auth_server]["password"] == base64.b64encode(
        password.encode("utf-8")
    ).decode("utf-8")

    res = create("-c", basic_auth_server, "testpkg")
    print(res)
