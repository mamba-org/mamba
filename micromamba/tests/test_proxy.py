import os
import shutil
import subprocess
import time
import urllib.parse
from pathlib import Path

import pytest

from . import helpers

__this_dir__ = Path(__file__).parent.resolve()


@pytest.fixture
def mitmdump_exe():
    """Get the path to the ``mitmdump`` executable.

    If the executable is provided in a conda environment, this fixture needs to be called
    before ``tmp_root_prefix`` and the like, as they will clean the ``PATH``.
    """
    return Path(shutil.which("mitmdump")).resolve()


class MitmProxy:
    def __init__(self, exe: Path, conf: Path, dump: Path):
        self.exe = Path(exe).resolve()
        self.conf = Path(conf).resolve()
        self.dump = Path(dump).resolve()
        self.process = None

    def start_proxy(self, port, options=[]):
        assert self.process is None
        self.process = subprocess.Popen(
            [
                self.exe,
                "--listen-port",
                str(port),
                "--scripts",
                str(__this_dir__ / "dump_proxy_connections.py"),
                "--set",
                f"outfile={self.dump}",
                "--set",
                f"confdir={self.conf}",
                *options,
            ]
        )

        # Wait until mitmproxy has generated its certificate or some tests might fail
        while not (Path(self.conf) / "mitmproxy-ca-cert.pem").exists():
            time.sleep(1)

    def stop_proxy(self):
        self.process.terminate()
        try:
            self.process.wait(3)
        except subprocess.TimeoutExpired:
            self.process.kill()
        self.process = None


@pytest.mark.parametrize("auth", [None, "foo:bar", "user%40example.com:pass"])
@pytest.mark.parametrize("ssl_verify", (True, False))
def test_proxy_install(
    mitmdump_exe, tmp_home, tmp_prefix, tmp_path, unused_tcp_port, auth, ssl_verify
):
    """
    This test makes sure micromamba follows the proxy settings in .condarc

    It starts mitmproxy with the `dump_proxy_connections.py` script, which dumps all requested urls in a text file.
    After that micromamba is used to install a package, while pointing it to that mitmproxy instance. Once
    micromamba finished the proxy server is stopped and the urls micromamba requested are compared to the urls
    mitmproxy intercepted, making sure that all the requests went through the proxy.
    """

    if auth is not None:
        proxy_options = ["--proxyauth", urllib.parse.unquote(auth)]
        proxy_url = "http://{}@localhost:{}".format(auth, unused_tcp_port)
    else:
        proxy_options = []
        proxy_url = "http://localhost:{}".format(unused_tcp_port)

    proxy = MitmProxy(
        exe=mitmdump_exe,
        conf=(tmp_path / "mitmproxy-conf"),
        dump=(tmp_path / "mitmproxy-dump"),
    )
    proxy.start_proxy(unused_tcp_port, proxy_options)

    rc_file = tmp_prefix / "rc.yaml"
    verify_string = proxy.conf / "mitmproxy-ca-cert.pem" if ssl_verify else "false"

    file_content = [
        "proxy_servers:",
        "    http: {}".format(proxy_url),
        "    https: {}".format(proxy_url),
        "ssl_verify: {}".format(verify_string),
    ]
    with open(rc_file, "w") as f:
        f.write("\n".join(file_content))

    cmd = ["xtensor", "--rc-file", rc_file]
    if os.name == "nt":
        # The certificates generated by mitmproxy don't support revocation.
        # The schannel backend curl uses on Windows fails revocation check if revocation isn't supported. Other
        # backends succeed revocation check in that case.
        cmd += ["--ssl-no-revoke"]

    res = helpers.install(*cmd, "--json", no_rc=False)

    proxy.stop_proxy()

    with open(proxy.dump, "r") as f:
        proxied_requests = f.read().splitlines()

    for fetch in res["actions"]["FETCH"]:
        assert fetch["url"] in proxied_requests
