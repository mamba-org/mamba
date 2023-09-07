import json
import os
import platform
import random
import shutil
import string
import subprocess
import uuid

from distutils.version import StrictVersion

import pytest

from helpers_mamba import (
    create,
    get_umamba,
    info,
    install,
    umamba_run,
    update,
)

from utils import (
    add_glibc_virtual_package,
    config_file,
    copy_channels_osx,
    run,
)

#from utils import (
    #Environment,
    #add_glibc_virtual_package,
    #copy_channels_osx,
    #platform_shells,
    #run_mamba_conda,
#)


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
    run(get_umamba(), channels, package)

    package = "b"
    run(get_umamba(), channels, package)

    channels = channels[::-1]
    run(get_umamba(), channels, package)



@pytest.fixture()
def temp_env_prefix():
    previous_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    previous_prefix = os.environ["CONDA_PREFIX"]

    env_name = random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    os.environ["MAMBA_ROOT_PREFIX"] = root_prefix
    create("-p", prefix, "-q")

    yield prefix

    shutil.rmtree(prefix)
    os.environ["MAMBA_ROOT_PREFIX"] = previous_root_prefix
    os.environ["CONDA_PREFIX"] = previous_prefix


def test_update(temp_env_prefix):
    # Check updating a package when a newer version
    # First install an older version (in fixture)
    version = "1.25.11"

    install("-p", temp_env_prefix, "-q", f"urllib3={version}", "-c", "conda-forge")

    res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import urllib3; print(urllib3.__version__)"
        )

    # Check that the installed version is the old one
    assert res.strip() == version

    # Update package
    update("-p", temp_env_prefix, "-q", "urllib3", "-c", "conda-forge")
    res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import urllib3; print(urllib3.__version__)"
        )
    # Check that the installed version is newer
    assert StrictVersion(res.strip()) > StrictVersion(version)

# TODO Missing feature in micromamba
#def test_env_update(temp_env_prefix, tmpdir):
    ## check updating a package when a newer version
    ## first install an older version
    #version = "1.25.11"
    #install("-p", temp_env_prefix, "-q", f"urllib3={version}", "-c", "conda-forge")
    #config_a = tmpdir / "a.yml"
    #config_a.write(
        #f"""
        #dependencies:
            #- python
            #- urllib3={version}
        #"""
    #)

    ## TODO to be replaced with corresponding umamba fct when feature will be available
    ##env.mamba(f"env update -q -f {config_a}")

    #res = umamba_run(
            #"-p", temp_env_prefix, "python", "-c", "import urllib3; print(urllib3.__version__)"
        #)

    ## check that the installed version is the old one
    #assert res.strip() == version

    ## then release the pin
    #config_b = tmpdir / "b.yml"
    #config_b.write(
        #"""
        #dependencies:
            #- urllib3
        #"""
    #)

    ## TODO to be replaced with corresponding umamba fct when feature will be available
    ##env.mamba(f"env update -q -f {config_b}")

    #res = umamba_run(
            #"-p", temp_env_prefix, "python", "-c", "import urllib3; print(urllib3.__version__)"
        #)
    ## check that the installed version is newer
    #assert StrictVersion(res.strip()) > StrictVersion(version)


def test_track_features(temp_env_prefix):
    # should install CPython since PyPy has track features
    version = "3.7.9"
    install("-p", temp_env_prefix, "-q", f"python={version}", "-c", "conda-forge", "--strict-channel-priority")
    res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import sys; print(sys.version)"
        )

    if platform.system() == "Windows":
        assert res.strip().startswith(version)
        assert "[MSC v." in res.strip()
    elif platform.system() == "Linux":
        assert res.strip().startswith(version)
        assert "[GCC" in res.strip()
    else:
        assert res.strip().startswith(version)
        assert "[Clang" in res.strip()

    if platform.system() == "Linux":
        # now force PyPy install
        install("-p", temp_env_prefix, "-q", f"python={version}=*pypy", "-c", "conda-forge", "--strict-channel-priority")
        res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import sys; print(sys.version)"
        )
        assert res.strip().startswith(version)
        assert "[PyPy" in res.strip()

# NOTE used to test --mamba-experimental flag as well (do we need to add it in micromamba? I don't think it's really useful...)
@pytest.mark.parametrize("use_json", [True, False])
def test_create_dry_run(use_json, tmpdir):
    env_dir = tmpdir / str(uuid.uuid1())

    cmd = ["-p", env_dir, "--dry-run", "python=3.8", "-c", "conda-forge"]

    if use_json:
        cmd += ["--json"]

    res = create(*cmd)

    if not use_json:
        # Assert the non-JSON output is in the terminal.
        assert "Total download" in res

    # dry-run, shouldn't create an environment
    assert not env_dir.check()


def test_create_subdir(tmpdir):
    env_dir = tmpdir / str(uuid.uuid1())

    with pytest.raises(subprocess.CalledProcessError) as e:
        create("-p", env_dir, "--dry-run", "--json", f"conda-forge/noarch::xtensor")


def test_create_files(tmpdir):
    """Check that multiple --file arguments are respected."""
    (tmpdir / "1.txt").write(b"a")
    (tmpdir / "2.txt").write(b"b")
    res = create("-p",
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
                 str(tmpdir / "2.txt"))

    names = {x["name"] for x in res["actions"]["FETCH"]}
    assert names == {"a", "b"}


def test_empty_create():
    res1 = create("-n", "test_env_xyz_i")
    res2 = create("-n", "test_env_xyz_ii", "--dry-run")


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
            get_umamba(),
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
        assert pkg["url"].startswith("https://conda.anaconda.org/conda-forge")


@pytest.mark.parametrize("config_file", [multichannel_config], indirect=["config_file"])
def test_multi_channels_with_subdir(config_file, tmpdir):
    # we need to create a config file first
    call_env = os.environ.copy()
    call_env["CONDA_PKGS_DIRS"] = str(tmpdir / random_string())
    with pytest.raises(subprocess.CalledProcessError) as e:
        output = subprocess.check_output(
            [
                get_umamba(),
                "create",
                "-n",
                "multichannels_with_subdir",
                "conda-forge2/noarch::xtensor",
                "--dry-run",
                "--json",
            ],
            env=call_env,
        )


def test_update_py(temp_env_prefix):
    # check updating a package when a newer version
    install("-p", temp_env_prefix, "-q", "python=3.8", "pip", "-c", "conda-forge")
    res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import sys; print(sys.version)"
        )
    assert "3.8" in res

    res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import pip; print(pip.__version__)"
        )
    assert len(res)

    install("-p", temp_env_prefix, "-q", "python=3.9", "-c", "conda-forge")
    res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import sys; print(sys.version)"
        )
    assert "3.9" in res
    res = umamba_run(
            "-p", temp_env_prefix, "python", "-c", "import pip; print(pip.__version__)"
        )
    assert len(res)


def test_unicode(tmpdir):
    uc = "320 áγђß家固êôōçñ한"
    res = create("-p", str(tmpdir / uc), "--json", "xtensor", "-c", "conda-forge")
    assert res["actions"]["PREFIX"] == str(tmpdir / uc)

    import libmambapy

    pd = libmambapy.PrefixData(str(tmpdir / uc))
    assert len(pd.package_records) > 1
    assert "xtl" in pd.package_records
    assert "xtensor" in pd.package_records


@pytest.mark.parametrize("use_json", [True, False])
def test_info(use_json):
    cmd = []
    if use_json:
        cmd += ["--json"]

    res = info(*cmd)

    print("RES OF INFO: ", res)

    from libmambapy.__version__ as version # NOTE use version of umamba instead

    if use_json:
        assert res["version"] == version
    else:
        assert version in res
