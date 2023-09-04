import uuid
from sys import platform

from helpers_mamba import create, install

def test_create(tmpdir):
    env_name = str(uuid.uuid1())
    env_dir = tmpdir / "envs" / env_name

    create("-p", env_dir, "python=3.8", "traitlets", "-c", "conda-forge")

    assert env_dir.check()
    if platform == "win32":
        assert (env_dir / "lib" / "site-packages" / "traitlets").check()
    else:
        assert (env_dir / "lib" / "python3.8" / "site-packages" / "traitlets").check()


def test_install(tmpdir):
    env_name = str(uuid.uuid1())
    env_dir = tmpdir / "envs" / env_name

    create("-p", env_dir, "python=3.8", "-c", "conda-forge")
    install("-p", env_dir, "nodejs", "-c", "conda-forge")

    assert env_dir.check()
    if platform == "win32":
        assert (env_dir / "lib" / "site-packages").check()
    else:
        assert (env_dir / "lib" / "python3.8" / "site-packages").check()
