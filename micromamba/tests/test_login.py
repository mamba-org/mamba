import base64
import json
import sys
from pathlib import Path

import pytest
from xprocess import ProcessStarter

from .helpers import create as umamba_create
from .helpers import login, logout, random_string


__this_dir__ = Path(__file__).resolve().parent

server_dir = __this_dir__ / "test-server"
pyserver = server_dir / "reposerver.py"
channel_a_directory = server_dir / "channel_a"
channel_b_directory = server_dir / "channel_b"
channel_r_directory = server_dir / "repo"

print(pyserver)

assert pyserver.exists()


@pytest.fixture
def auth_file(tmp_home):
    return tmp_home / ".mamba/auth/authentication.json"


def reposerver_multi(
    xprocess,
    channels,
    port=1234,
):
    computed_args = [sys.executable, "-u", pyserver, "--port", port]
    for channel in channels:
        computed_args += [
            "--directory",
            channel.get("directory", channel_a_directory),
        ]
        if "name" in channel:
            computed_args += ["--name", channel["name"]]
        auth = channel["auth"]
        if auth == "token":
            computed_args += ["--token", channel["token"]]
        elif auth == "bearer":
            computed_args += ["--bearer", channel["token"]]
        elif auth == "basic":
            computed_args += [
                "--user",
                channel["user"],
                "--password",
                channel["password"],
            ]
        else:
            raise ValueError("Wrong authentication method")
        computed_args += ["--"]

    class Starter(ProcessStarter):
        # startup pattern
        pattern = "Server started at localhost:"

        # command to start process
        args = computed_args

    # ensure process is running and return its logfile
    xprocess.ensure("reposerver", Starter)

    # create a connection or url/port info to the server
    conn = f"http://localhost:{port}"

    yield conn

    # clean up whole process tree afterwards
    xprocess.getinfo("reposerver").terminate()


def reposerver_single(xprocess, port=1234, **kwargs):
    yield from reposerver_multi(channels=[kwargs], xprocess=xprocess, port=port)


@pytest.fixture
def token_server(token, xprocess):
    yield from reposerver_single(xprocess, auth="token", token=token)


@pytest.fixture
def bearer_server(token, xprocess):
    yield from reposerver_single(xprocess, auth="bearer", token=token)


@pytest.fixture
def basic_auth_server(user, password, xprocess):
    yield from reposerver_single(xprocess, auth="basic", user=user, password=password)


@pytest.fixture
def multi_server(xprocess, channels):
    yield from reposerver_multi(xprocess, channels=channels)


def create(*in_args, folder=None, root=None, override_channels=True):
    args = [arg for arg in in_args]
    args += ["-vvv"]
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


def remove_url_scheme(url: str) -> str:
    return url.removeprefix("https://").removeprefix("http://").removeprefix("file://")


@pytest.mark.parametrize(
    "login_1,token_1",
    [
        ("https://myserver.com:1234", "mytoken"),
        ("http://myserver.com", "4320nksdf"),
        ("myserver.com:1234", "AD9sd55"),
    ],
)
@pytest.mark.parametrize(
    "login_2, token_2",
    [
        ("https://myserver2.com:1234", "othertoken"),
        ("myserver.com:999", "hfijwr4"),
        ("myserver2.com/channel", "453"),
    ],
)
def test_login_logout(auth_file, login_1, token_1, login_2, token_2):
    login(login_1, "--token", token_1)
    login(login_2, "--token", token_2)

    with open(auth_file) as fi:
        data = json.load(fi)

    for login_, token in [(login_1, token_1), (login_2, token_2)]:
        login_id = remove_url_scheme(login_)
        assert login_id in data
        assert data[login_id]["token"] == token

    logout(login_1)

    with open(auth_file) as fi:
        data = json.load(fi)

    assert remove_url_scheme(login_1) not in data


@pytest.mark.parametrize("token", ["crazytoken1234"])
def test_token(auth_file, token, token_server):
    login(token_server, "--token", token)
    with open(auth_file) as fi:
        data = json.load(fi)

    token_server_id = remove_url_scheme(token_server)
    assert token_server_id in data
    assert data[token_server_id]["token"] == token

    res = create("-c", token_server, "testpkg", "--json")
    pkg = res["actions"]["FETCH"][0]
    assert pkg["name"] == "testpkg"


@pytest.mark.parametrize("user,password", [["testuser", "xyzpass"]])
def test_basic_auth(auth_file, user, password, basic_auth_server):
    login(basic_auth_server, "--username", user, "--password", password)
    with open(auth_file) as fi:
        data = json.load(fi)

    basic_auth_server_id = remove_url_scheme(basic_auth_server)
    assert basic_auth_server_id in data
    assert data[basic_auth_server_id]["password"] == base64.b64encode(
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
def test_basic_auth_explicit_txt(auth_file, user, password, basic_auth_server, tmp_path):
    login(basic_auth_server, "--username", user, "--password", password)

    env_file = tmp_path / "environment.txt"
    env_file.write_text(env_file_content.format(server=basic_auth_server))
    env_folder = tmp_path / "env"
    root_folder = tmp_path / "root"
    create("-f", str(env_file), folder=env_folder, root=root_folder)

    assert (env_folder / "conda-meta" / "_r-mutex-1.0.1-anacondar_1.json").exists()


@pytest.mark.parametrize("user,password", [["testuser", "xyzpass"]])
def test_basic_auth_explicit_yaml(auth_file, user, password, basic_auth_server, tmp_path):
    login(basic_auth_server, "--username", user, "--password", password)

    env_file = tmp_path / "environment.yml"
    env_file.write_text(env_yaml_content.format(server=basic_auth_server))
    env_folder = tmp_path / "env"
    root_folder = tmp_path / "root"
    create(
        "-f",
        str(env_file),
        folder=env_folder,
        root=root_folder,
        override_channels=False,
    )

    assert (env_folder / "conda-meta" / "_r-mutex-1.0.1-anacondar_1.json").exists()


@pytest.mark.parametrize("token", ["randomverystrongtoken"])
def test_token_explicit(auth_file, token, token_server, tmp_path):
    login(token_server, "--token", token)

    env_file = tmp_path / "environment.txt"
    env_file.write_text(env_file_content.format(server=token_server))
    env_folder = tmp_path / "env"
    root_folder = tmp_path / "root"
    create("-f", str(env_file), folder=env_folder, root=root_folder)

    assert (env_folder / "conda-meta" / "_r-mutex-1.0.1-anacondar_1.json").exists()


@pytest.mark.parametrize("token", ["randomverystrongtoken"])
def test_bearer_explicit(auth_file, token, bearer_server, tmp_path):
    login(bearer_server, "--bearer", token)

    host = remove_url_scheme(bearer_server)
    with open(auth_file) as fi:
        data = json.load(fi)
        assert data[host]["token"] == token
        assert data[host]["type"] == "BearerToken"
        print(data)

    env_file = tmp_path / "environment.txt"
    env_file.write_text(env_file_content.format(server=bearer_server))
    env_folder = tmp_path / "env"
    root_folder = tmp_path / "root"
    create("-f", str(env_file), folder=env_folder, root=root_folder)

    assert (env_folder / "conda-meta" / "_r-mutex-1.0.1-anacondar_1.json").exists()


@pytest.mark.parametrize(
    "channels",
    [
        [
            {
                "directory": channel_r_directory,
                "name": "defaults",
                "auth": "token",
                "token": "randomverystrongtoken",
            },
            {
                "directory": channel_a_directory,
                "name": "channel_a",
                "auth": "basic",
                "user": "testuser",
                "password": "xyzpass",
            },
        ]
    ],
)
def test_login_multi_channels(auth_file, channels, multi_server):
    channel_1, channel_2 = channels
    for chan in channels:
        chan_url = f"{multi_server}/{chan['name']}"
        if chan["auth"] == "token":
            login(chan_url, "--token", chan["token"])
        elif chan["auth"] == "basic":
            login(
                chan_url,
                "--username",
                chan["user"],
                "--password",
                chan["password"],
            )
        else:
            raise ValueError(f"Invalid auth method: {chan['auth']}")

    with open(auth_file) as fi:
        data = json.load(fi)

    channel_1_url = f"{multi_server}/{channel_1['name']}"
    assert remove_url_scheme(channel_1_url) in data
    channel_2_url = f"{multi_server}/{channel_2['name']}"
    assert remove_url_scheme(channel_2_url) in data

    create("-c", channel_1_url, "test-package", override_channels=True)
    create("-c", channel_2_url, "_r-mutex", override_channels=True)
