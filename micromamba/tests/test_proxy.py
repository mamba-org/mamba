import asyncio
import os
import shutil
import time
from pathlib import Path
from subprocess import TimeoutExpired

from .helpers import *


class TestProxy:

    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    mitm_exe = shutil.which("mitmdump")
    mitm_confdir = os.path.join(root_prefix, "mitmproxy")
    mitm_dump_path = os.path.join(root_prefix, "dump.json")

    proxy_process = None

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestProxy.root_prefix
        os.environ["CONDA_PREFIX"] = TestProxy.prefix

    def setup_method(self):
        create("-n", TestProxy.env_name, "--offline", no_dry_run=True)

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestProxy.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestProxy.current_prefix

    def teardown_method(self):
        shutil.rmtree(TestProxy.root_prefix)

    def start_proxy(self, port, options=[]):
        assert self.proxy_process is None
        script = Path(__file__).parent / "dump_proxy_connections.py"
        self.proxy_process = subprocess.Popen(
            [
                TestProxy.mitm_exe,
                "--listen-port",
                str(port),
                "-s",
                script,
                "--set",
                f"outfile={TestProxy.mitm_dump_path}",
                "--set",
                f"confdir={TestProxy.mitm_confdir}",
                *options,
            ]
        )

        # Wait until mitmproxy has generated its certificate or some tests might fail
        while not (Path(TestProxy.mitm_confdir) / "mitmproxy-ca-cert.pem").exists():
            time.sleep(1)

    def stop_proxy(self):
        self.proxy_process.terminate()
        try:
            self.proxy_process.wait(3)
        except TimeoutExpired:
            self.proxy_process.kill()
        self.proxy_process = None

    @pytest.mark.parametrize("with_auth", (True, False))
    @pytest.mark.parametrize("ssl_verify", (True, False))
    def test_install(self, unused_tcp_port, with_auth, ssl_verify):
        if with_auth:
            options = ["--proxyauth", "foo:bar"]
            proxy_url = "http://foo:bar@localhost:{}".format(unused_tcp_port)
        else:
            options = []
            proxy_url = "http://localhost:{}".format(unused_tcp_port)

        self.start_proxy(unused_tcp_port, options)

        cmd = ["xtensor"]
        f_name = random_string() + ".yaml"
        rc_file = os.path.join(TestProxy.prefix, f_name)

        if ssl_verify:
            verify_string = os.path.abspath(
                os.path.join(TestProxy.mitm_confdir, "mitmproxy-ca-cert.pem")
            )
        else:
            verify_string = "false"

        file_content = [
            "proxy_servers:",
            "    http: {}".format(proxy_url),
            "    https: {}".format(proxy_url),
            "ssl_verify: {}".format(verify_string),
        ]
        with open(rc_file, "w") as f:
            f.write("\n".join(file_content))

        cmd += ["--rc-file", rc_file]
        res = install(*cmd, "--json", no_rc=False)

        self.stop_proxy()

        with open(TestProxy.mitm_dump_path, "r") as f:
            proxied_requests = f.read().splitlines()

        for fetch in res["actions"]["FETCH"]:
            assert fetch["url"] in proxied_requests
