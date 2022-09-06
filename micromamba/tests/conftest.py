import os
import pathlib
from typing import Generator

import pytest

from . import helpers


@pytest.fixture
def tmp_home(tmp_path: pathlib.Path) -> Generator[pathlib.Path, None, None]:
    """Change the home directory to a tmp folder for the duration of a test."""
    # Try multiple combination for Unix/Windows
    home_envs = [k for k in ("HOME", "USERPROFILE") if k in os.environ]
    old_homes = {name: os.environ[name] for name in home_envs}

    if len(home_envs) > 0:
        new_home = tmp_path / "home"
        new_home.mkdir(parents=True, exist_ok=True)
        for env in home_envs:
            os.environ[env] = str(new_home)
        yield new_home
        for env, home in old_homes.items():
            os.environ[env] = home
    else:
        yield pathlib.Path.home()


@pytest.fixture(scope="session")
def tmp_pkgs_dirs(tmp_path_factory: pytest.TempPathFactory) -> pathlib.Path:
    """A common package cache for mamba downloads.

    The directory is not used automatically when calling this fixture.
    """
    return tmp_path_factory.mktemp("pkgs_dirs")


@pytest.fixture(params=[False])
def shared_pkgs_dirs(request) -> bool:
    """A dummy fixture to control the use of shared package dir."""
    return request.param


@pytest.fixture
def tmp_clean_env(
    tmp_pkgs_dirs: pathlib.Path, shared_pkgs_dirs: bool
) -> Generator[None, None, None]:
    """Remove all Conda/Mamba activation artifacts from environment."""
    saved_environ = {}
    for k, v in os.environ.items():
        if k.startswith(("CONDA", "_CONDA", "MAMBA", "_MAMBA")):
            saved_environ[k] = v
            del os.environ[k]

    def keep_in_path(
        p: str, prefix: str | None = saved_environ.get("CONDA_PREFIX")
    ) -> bool:
        if "condabin" in p:
            return False
        if prefix is not None:
            p = str(pathlib.Path(p).expanduser().resolve())
            prefix = str(pathlib.Path(prefix).expanduser().resolve())
            return not p.startswith(prefix)
        return True

    path_list = os.environ["PATH"].split(os.pathsep)
    path_list = [p for p in path_list if keep_in_path(p)]
    os.environ["PATH"] = os.pathsep.join(path_list)

    if shared_pkgs_dirs:
        os.environ["CONDA_PKGS_DIRS"] = str(tmp_pkgs_dirs)

    yield None

    os.environ.update(saved_environ)


@pytest.fixture(
    params=[helpers.random_string, "some ™∞¢3 spaces §∞©ƒ√≈ç", "long_prefix_" * 20]
)
def tmp_env_name(request) -> str:
    """Return the explicit or implicit parametrization."""
    if callable(request.param):
        return request.param()
    return request.param


@pytest.fixture
def tmp_root_prefix(
    tmp_path: pathlib.Path, tmp_clean_env: None
) -> Generator[pathlib.Path, None, None]:
    """Change the micromamba root directory to a tmp folder for the duration of a test."""
    old_root_prefix = os.environ.get("MAMBA_ROOT_PREFIX")
    new_root_prefix = tmp_path / "mamba"
    new_root_prefix.mkdir(parents=True, exist_ok=True)
    os.environ["MAMBA_ROOT_PREFIX"] = str(new_root_prefix)
    yield new_root_prefix
    if old_root_prefix is not None:
        os.environ["MAMBA_ROOT_PREFIX"] = old_root_prefix
    else:
        del os.environ["MAMBA_ROOT_PREFIX"]


@pytest.fixture
def tmp_empty_env(
    tmp_root_prefix: pathlib.Path, tmp_env_name: str
) -> Generator[pathlib.Path, None, None]:
    """An empty envirnment created under a temporary root prefix."""
    helpers.create("-n", tmp_env_name, no_dry_run=True)
    yield tmp_root_prefix


@pytest.fixture
def tmp_prefix(
    tmp_root_prefix: pathlib.Path, tmp_env_name: str
) -> Generator[pathlib.Path, None, None]:
    """Change the conda prefix to a tmp folder for the duration of a test."""
    old_prefix = os.environ.get("CONDA_PREFIX")
    new_prefix = tmp_root_prefix / "envs" / tmp_env_name
    new_prefix.mkdir(parents=True, exist_ok=True)
    os.environ["CONDA_PREFIX"] = str(new_prefix)
    yield new_prefix
    if old_prefix is not None:
        os.environ["CONDA_PREFIX"] = old_prefix
    else:
        del os.environ["CONDA_PREFIX"]
