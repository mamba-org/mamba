import uuid

from mamba.api import create, install


def test_create(tmpdir):
    env_name = str(uuid.uuid1())
    env_dir = tmpdir / "envs" / env_name

    create(env_name, ["python=3.8", "traitlets"], ["conda-forge"], base_prefix=tmpdir)

    assert env_dir.check()
    assert (env_dir / "lib" / "python3.8" / "site-packages" / "traitlets").check()


def test_install(tmpdir):
    env_name = str(uuid.uuid1())
    env_dir = tmpdir / "envs" / env_name

    create(env_name, ["python=3.8"], ["conda-forge"], base_prefix=tmpdir)
    install(env_name, ["nodejs"], ["conda-forge"], base_prefix=tmpdir)

    assert env_dir.check()
    assert (env_dir / "lib" / "python3.8" / "site-packages").check()
    assert (env_dir / "bin" / "node").check()
