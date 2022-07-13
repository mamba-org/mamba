import os
import pathlib
import random
import string
from typing import Generator

import pytest


@pytest.fixture
def env_name(N: int = 10) -> str:
    """Return Ten random characters."""
    return "".join(random.choices(string.ascii_uppercase + string.digits, k=N))


@pytest.fixture
def tmp_home(tmp_path: pathlib.Path) -> Generator[pathlib.Path, None, None]:
    """Change the home directory to a tmp folder for the duration of a test."""
    old_home = os.environ.get("HOME")
    new_home = tmp_path / "home"
    new_home.mkdir(parents=True, exist_ok=True)
    if old_home is not None:
        os.environ["HOME"] = str(new_home)
        yield new_home
        os.environ["HOME"] = old_home
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
    tmp_root_prefix: pathlib.Path, env_name: str
) -> Generator[pathlib.Path, None, None]:
    """Change the conda prefix to a tmp folder for the duration of a test."""
    old_prefix = os.environ.get("CONDA_PREFIX")
    new_prefix = tmp_root_prefix / "envs" / env_name
    new_prefix.mkdir(parents=True, exist_ok=True)
    os.environ["CONDA_PREFIX"] = str(new_prefix)
    yield new_prefix
    if old_prefix is not None:
        os.environ["CONDA_PREFIX"] = old_prefix
    else:
        del os.environ["CONDA_PREFIX"]
