import os
import urllib.request
import subprocess
import json

from utils import Environment


def setup_function(function):
    os.makedirs("test/channel_zchunk", exist_ok=True)
    function.server_proc = subprocess.Popen("python -m RangeHTTPServer".split(), cwd="test/channel_zchunk")


def teardown_function(function):
    function.server_proc.kill()


def test_zchunk():
    for arch in ("linux-64", "noarch"):
        if not os.path.exists(f"test/channel_zchunk/{arch}/repodata.json.zck"):
            os.makedirs(f"test/channel_zchunk/{arch}", exist_ok=True)
            urllib.request.urlretrieve(f"https://conda.anaconda.org/conda-forge/{arch}/repodata.json", f"test/channel_zchunk/{arch}/repodata.json")
            subprocess.check_call(["zck", f"test/channel_zchunk/{arch}/repodata.json", "-o", f"test/channel_zchunk/{arch}/repodata.json.zck"])
            os.remove(f"test/channel_zchunk/{arch}/repodata.json")

    with Environment() as env:
        # first download should download all chunks
        out = env.execute(f"$MAMBA install --zchunk --override-channels --strict-channel-priority --dry-run -v -y xtensor -c http://127.0.0.1:8000")
        lines = [l for l in out if "Missing chunks" in l]
        missing_chunks = [int(l[l.rfind(":") + 1:]) for l in lines]
        assert 0 not in missing_chunks

        # second download should not download any chunk
        out = env.execute(f"$MAMBA install --zchunk --override-channels --strict-channel-priority --dry-run -v -y xtensor -c http://127.0.0.1:8000")
        lines = [l for l in out if "Missing chunks" in l]
        missing_chunks = [int(l[l.rfind(":") + 1:]) for l in lines]
        assert all(v == 0 for v in missing_chunks)
