from distutils.version import StrictVersion
from pathlib import Path
from subprocess import STDOUT

from utils import Environment, add_glibc_virtual_package, copy_channels_osx, run_mamba_conda

import json
import pytest
import subprocess
import uuid


def test_install():
    add_glibc_virtual_package()
    copy_channels_osx()
    test_folder = Path(__file__).parent

    channels = [str(test_folder / 'channel_a'), str(test_folder / 'channel_b')]
    package = 'a'
    run_mamba_conda(channels, package)

    package = 'b'
    run_mamba_conda(channels, package)

    channels = channels[::-1]
    run_mamba_conda(channels, package)


def test_update():
    # check updating a package when a newer version
    with Environment() as env:
        # first install an older version
        version = '1.25.7'
        env.execute(f'$MAMBA install -q -y urllib3={version}')
        out = env.execute('python -c "import urllib3; print(urllib3.__version__)"')
        # check that the installed version is the old one
        assert out[-1] == version

        # then update package
        env.execute('$MAMBA update -q -y urllib3')
        out = env.execute('python -c "import urllib3; print(urllib3.__version__)"')
        # check that the installed version is newer
        assert StrictVersion(out[-1]) > StrictVersion(version)


def test_track_features():
    with Environment() as env:
        # should install CPython since PyPy has track features
        version = '3.6.9'
        env.execute(f'$MAMBA install -q -y "python={version}" --strict-channel-priority')
        out = env.execute('python -c "import sys; print(sys.version)"')
        assert out[-2].startswith(version)
        assert out[-1].startswith('[GCC')

        # now force PyPy install
        env.execute(f'$MAMBA install -q -y "python={version}=*pypy" --strict-channel-priority')
        out = env.execute('python -c "import sys; print(sys.version)"')
        assert out[-2].startswith(version)
        assert out[-1].startswith('[PyPy')


@pytest.mark.parametrize('experimental', [True, False])
@pytest.mark.parametrize('use_json', [True, False])
def test_create_dry_run(experimental, use_json, tmpdir):
    env_dir = tmpdir / str(uuid.uuid1())

    mamba_cmd = "mamba create --dry-run -y"
    if experimental:
        mamba_cmd += " --mamba-experimental"
    if use_json:
        mamba_cmd += " --json"
    mamba_cmd += f" -p {env_dir} python=3.8"

    output = subprocess.check_output(mamba_cmd, shell=True).decode()

    if use_json:
        # assert that the output is parseable parseable json
        json.loads(output)
    elif not experimental:
        # Assert the non-JSON output is in the terminal.
        assert "Total download" in output

    if not experimental:
        # dry-run, shouldn't create an environment
        assert not env_dir.check()


def test_create_files(tmpdir):
    """Check that multiple --file arguments are respected."""
    (tmpdir / "1.txt").write(b"a")
    (tmpdir / "2.txt").write(b"b")
    output = subprocess.check_output([
        'mamba',
        'create',
        '-p',
        str(tmpdir / 'env'),
        '--json',
        '--override-channels',
        '--strict-channel-priority',
        '--dry-run',
        '-c',
        './test/channel_b',
        '-c',
        './test/channel_a',
        '--file',
        str(tmpdir / "1.txt"),
        '--file',
        str(tmpdir / "2.txt")
    ])
    output = json.loads(output)
    names = {x['name'] for x in output['actions']['FETCH']}
    assert names == {'a', 'b'}
