import os
import pathlib
import random
import string
from typing import Generator

import pytest


def random_str(n: int = 10) -> str:
    """Return random characters and digits."""
    return "".join(random.choices(string.ascii_uppercase + string.digits, k=n))


@pytest.fixture(params=[random_str, "some ™∞¢3 spaces §∞©ƒ√≈ç", "long_prefix_" * 20])
def tmp_env_name(request) -> str:
    """Return the explicit or implicit parametrization."""
    if callable(request.param):
        return request.param()
    return request.param


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


@pytest.fixture
def tmp_root_prefix(tmp_path: pathlib.Path) -> Generator[pathlib.Path, None, None]:
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
