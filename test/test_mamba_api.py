import io
import pytest
import mamba_api as api
from utils import load_channels
from contextlib import redirect_stdout

def test_redirect():
    api_ctx = api.Context()
    api_ctx.json = True
    api_ctx.dry_run = True
    api_ctx.set_verbosity(2)
    api_ctx.use_only_tar_bz2 = True
    api_ctx.always_yes = True
    api_ctx.pkgs_dirs = self.pkg_dirs
    api_ctx.channels = []
    channels = ["./test/channel_b", "./test/channel_a"]
    pool = mamba_api.Pool()
    stream = io.StringIO()
    with redirect_stdout(stream):
        with api.ostream_redirect(stdout=true, stderr=true):
            load_channels(pool, channels, [], prepend=False)
    stdout, stderr = capfd.readouterr()
    assert stdout == ""
    assert stream.getvalue() == msg