import json
import os
import platform
import random
import shutil
import string
import subprocess
import uuid

import pytest
from packaging.version import Version
from utils import (
    Environment,
    add_glibc_virtual_package,
    config_file,
    copy_channels_osx,
    platform_shells,
    run_mamba_conda,
)


def random_string(N=10):
    return "".join(random.choices(string.ascii_uppercase + string.digits, k=N))


def test_install():
    add_glibc_virtual_package()
    copy_channels_osx()

    channels = [
        (".", "mamba", "tests", "channel_b"),
        (".", "mamba", "tests", "channel_a"),
    ]
    package = "a"
    run_mamba_conda(channels, package)

    package = "b"
    run_mamba_conda(channels, package)

    channels = channels[::-1]
    run_mamba_conda(channels, package)


@pytest.mark.parametrize("shell_type", platform_shells())
def test_update(shell_type):
    # check updating a package when a newer version
    with Environment(shell_type) as env:
        # first install an older version
        version = "2.0.0"
        env.mamba(f"install -q -y --override-channels -c conda-forge urllib3={version}")
        out = env.execute('python -c "import urllib3; print(urllib3.__version__)"')

        # check that the installed version is the old one
        assert out[-1] == version

        # then update package
        env.mamba("update --override-channels -c conda-forge -q -y urllib3")
        out = env.execute('python -c "import urllib3; print(urllib3.__version__)"')
        # check that the installed version is newer
        assert Version(out[-1]) > Version(version)


@pytest.mark.parametrize("shell_type", platform_shells())
def test_env_update(shell_type, tmpdir):
    # check updating a package when a newer version
    with Environment(shell_type) as env:
        # first install an older version
        version = "2.0.0"
        config_a = tmpdir / "a.yml"
        config_a.write(
            f"""
            channels:
              - conda-forge
            dependencies:
             - python
             - urllib3={version}
            """
        )
        env.mamba(f"env update -q -f {config_a}")
        out = env.execute('python -c "import urllib3; print(urllib3.__version__)"')

        # check that the installed version is the old one
        assert out[-1] == version

        # then release the pin
        config_b = tmpdir / "b.yml"
        config_b.write(
            """
            channels:
              - conda-forge
            dependencies:
             - urllib3
            """
        )
        env.mamba(f"env update  -q -f {config_b}")
        out = env.execute('python -c "import urllib3; print(urllib3.__version__)"')
        # check that the installed version is newer
        assert Version(out[-1]) > Version(version)


@pytest.mark.parametrize("shell_type", platform_shells())
def test_track_features(shell_type):
    with Environment(shell_type) as env:
        # should install CPython since PyPy has track features
        version = "3.7.9"
        env.mamba(
            f'install -q -y "python={version}" --strict-channel-priority -c conda-forge'
        )
        out = env.execute('python -c "import sys; print(sys.version)"')

        if platform.system() == "Windows":
            assert out[-1].startswith(version)
            assert "[MSC v." in out[-1]
        elif platform.system() == "Linux":
            assert out[-2].startswith(version)
            assert out[-1].startswith("[GCC")
        else:
            assert out[-2].startswith(version)
            assert out[-1].startswith("[Clang")

        if platform.system() == "Linux":
            # now force PyPy install
            env.mamba(
                f'install -q -y "python={version}=*pypy" --strict-channel-priority -c conda-forge'
            )
            out = env.execute('python -c "import sys; print(sys.version)"')
            assert out[-2].startswith(version)
            assert out[-1].startswith("[PyPy")


@pytest.mark.parametrize("experimental", [True, False])
@pytest.mark.parametrize("use_json", [True, False])
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


def test_create_subdir(tmpdir):
    env_dir = tmpdir / str(uuid.uuid1())

    try:
        output = subprocess.check_output(
            "mamba create --dry-run --json "
            f"-p {env_dir} "
            f"conda-forge/noarch::xtensor",
            shell=True,
        )
    except subprocess.CalledProcessError as e:
        result = json.loads(e.output)
        assert result["error"] == (
            'RuntimeError(\'The package "conda-forge/noarch::xtensor" is'
            " not available for the specified platform')"
        )


def test_create_files(tmpdir):
    """Check that multiple --file arguments are respected."""
    (tmpdir / "1.txt").write(b"a")
    (tmpdir / "2.txt").write(b"b")
    output = subprocess.check_output(
        [
            "mamba",
            "create",
            "-p",
            str(tmpdir / "env"),
            "--json",
            "--override-channels",
            "--strict-channel-priority",
            "--dry-run",
            "-c",
            "./mamba/tests/channel_b",
            "-c",
            "./mamba/tests/channel_a",
            "--file",
            str(tmpdir / "1.txt"),
            "--file",
            str(tmpdir / "2.txt"),
        ]
    )
    output = json.loads(output)
    names = {x["name"] for x in output["actions"]["FETCH"]}
    assert names == {"a", "b"}


def test_empty_create():
    output = subprocess.check_output(["mamba", "create", "-n", "test_env_xyz_i"])
    output2 = subprocess.check_output(
        ["mamba", "create", "-n", "test_env_xyz_ii", "--dry-run"]
    )


multichannel_config = {
    "channels": ["conda-forge"],
    "custom_multichannels": {"conda-forge2": ["conda-forge"]},
}


@pytest.mark.parametrize("config_file", [multichannel_config], indirect=["config_file"])
def test_multi_channels(config_file, tmpdir):
    # we need to create a config file first
    call_env = os.environ.copy()
    call_env["CONDA_PKGS_DIRS"] = str(tmpdir / random_string())
    output = subprocess.check_output(
        [
            "mamba",
            "create",
            "-n",
            "multichannels",
            "conda-forge2::xtensor",
            "--dry-run",
            "--json",
        ],
        env=call_env,
    )
    result = output.decode()
    print(result)
    res = json.loads(result)
    for pkg in res["actions"]["FETCH"]:
        assert pkg["channel"].startswith("https://conda.anaconda.org/conda-forge")
    for pkg in res["actions"]["LINK"]:
        assert pkg["base_url"] == "https://conda.anaconda.org/conda-forge"


@pytest.mark.parametrize("config_file", [multichannel_config], indirect=["config_file"])
def test_multi_channels_with_subdir(config_file, tmpdir):
    # we need to create a config file first
    call_env = os.environ.copy()
    call_env["CONDA_PKGS_DIRS"] = str(tmpdir / random_string())
    try:
        output = subprocess.check_output(
            [
                "mamba",
                "create",
                "-n",
                "multichannels_with_subdir",
                "conda-forge2/noarch::xtensor",
                "--dry-run",
                "--json",
            ],
            env=call_env,
        )
    except subprocess.CalledProcessError as e:
        result = json.loads(e.output)
        assert result["error"] == (
            'RuntimeError(\'The package "conda-forge2/noarch::xtensor" is'
            " not available for the specified platform')"
        )


def test_update_py():
    # check updating a package when a newer version
    if platform.system() == "Windows":
        shell_type = "cmd.exe"
    else:
        shell_type = "bash"

    with Environment(shell_type) as env:
        env.mamba(f'install -q -y "python=3.8" pip -c conda-forge')
        out = env.execute('python -c "import sys; print(sys.version)"')
        assert "3.8" in out[0]

        out = env.execute('python -c "import pip; print(pip.__version__)"')
        assert len(out)

        env.mamba(f'install -q -y "python=3.9" -c conda-forge')
        out = env.execute('python -c "import sys; print(sys.version)"')
        assert "3.9" in out[0]
        out = env.execute('python -c "import pip; print(pip.__version__)"')
        assert len(out)


def test_unicode(tmpdir):
    uc = "320 áγђß家固êôōçñ한"
    output = subprocess.check_output(
        ["mamba", "create", "-p", str(tmpdir / uc), "--json", "xtensor"]
    )
    output = json.loads(output)
    assert output["actions"]["PREFIX"] == str(tmpdir / uc)

    import libmambapy

    pd = libmambapy.PrefixData(str(tmpdir / uc))
    assert len(pd.package_records) > 1
    assert "xtl" in pd.package_records
    assert "xtensor" in pd.package_records


@pytest.mark.parametrize("use_json", [True, False])
def test_info(use_json):
    mamba_cmd = ["mamba", "info"]
    if use_json:
        mamba_cmd.append("--json")

    output = subprocess.check_output(mamba_cmd).decode()

    from mamba import __version__

    if use_json:
        output = json.loads(output)
        assert output["mamba_version"] == __version__
    else:
        assert "mamba version : " + __version__ in output


@pytest.mark.skipif(
    platform.system() != "Darwin",
    reason="__osx is a platform specific virtual package from conda-forge",
)
def test_remove_virtual_package():
    # non-regression test for https://github.com/mamba-org/mamba/issues/2129
    with Environment("bash") as env:
        # The virtual package __osx is installed by default on macOS:
        out = env.mamba("info -q")
        assert "__osx" in "\n".join(out)

        # Attempting to remove it should not fail (but should not remove it
        # either).
        env.mamba("remove -q -y __osx")
        out = env.mamba("info -q")
        assert "__osx" in "\n".join(out)
