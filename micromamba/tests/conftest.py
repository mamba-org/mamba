import copy
import os
import pathlib
import platform
from typing import Any, Generator, Mapping, Optional

import pytest

from . import helpers

####################
#  Config options  #
####################


def pytest_addoption(parser):
    """Add command line argument to pytest."""
    parser.addoption(
        "--mamba-pkgs-dir",
        action="store",
        default=None,
        help="Package cache to reuse between tests",
    )
    parser.addoption(
        "--no-eager-clean",
        action="store_true",
        default=False,
        help=(
            "Do not eagerly delete temporary folders such as HOME and MAMBA_ROOT_PREFIX"
            "created during tests."
            "These folders take a lot of disk space so we delete them eagerly."
            "For debugging, it can be convenient to keep them."
            "With this option, cleaning will fallback on the default pytest policy."
        ),
    )


##################
#  Test fixture  #
##################


@pytest.fixture(autouse=True)
def tmp_environ() -> Generator[Mapping[str, Any], None, None]:
    """Saves and restore environment variables.

    This is used for test that need to modify ``os.environ``
    """
    old_environ = copy.deepcopy(os.environ)
    yield old_environ
    os.environ.clear()
    os.environ.update(old_environ)


@pytest.fixture
def tmp_home(
    request, tmp_environ, tmp_path_factory: pytest.TempPathFactory
) -> Generator[pathlib.Path, None, None]:
    """Change the home directory to a tmp folder for the duration of a test."""
    # Try multiple combination for Unix/Windows
    home_envs = ["HOME", "USERPROFILE"]
    used_homes = [env for env in home_envs if env in os.environ]

    new_home = pathlib.Path.home()
    if len(used_homes) > 0:
        new_home = tmp_path_factory.mktemp("home")
        new_home.mkdir(parents=True, exist_ok=True)
        for env in used_homes:
            os.environ[env] = str(new_home)

    if platform.system() == "Windows":
        os.environ["APPDATA"] = str(new_home / "AppData" / "Roaming")
        os.environ["LOCALAPPDATA"] = str(new_home / "AppData" / "Local")

    yield new_home

    # Pytest would clean it automatically but this can be large (0.5 Gb for repodata)
    # We clean it explicitly
    if not request.config.getoption("--no-eager-clean"):
        try:
            helpers.rmtree(new_home)
        except PermissionError:
            pass


@pytest.fixture
def tmp_clean_env(tmp_environ: None) -> None:
    """Remove all Conda/Mamba activation artifacts from environment."""
    for k, v in os.environ.items():
        if k.startswith(("CONDA", "_CONDA", "MAMBA", "_MAMBA", "XDG_")):
            del os.environ[k]

    def keep_in_path(
        p: str, prefix: Optional[str] = tmp_environ.get("CONDA_PREFIX")
    ) -> bool:
        if "condabin" in p:
            return False
        # On windows, PATH is also used for dyanamic libraries.
        if (prefix is not None) and (platform.system() != "Windows"):
            p = str(pathlib.Path(p).expanduser().resolve())
            prefix = str(pathlib.Path(prefix).expanduser().resolve())
            return not p.startswith(prefix)
        return True

    path_list = os.environ["PATH"].split(os.pathsep)
    path_list = [p for p in path_list if keep_in_path(p)]
    os.environ["PATH"] = os.pathsep.join(path_list)
    # os.environ restored by tmp_clean_env and tmp_environ


@pytest.fixture(scope="session")
def tmp_pkgs_dirs(tmp_path_factory: pytest.TempPathFactory, request) -> pathlib.Path:
    """A common package cache for mamba downloads.

    The directory is not used automatically when calling this fixture.
    """
    if (p := request.config.getoption("--mamba-pkgs-dir")) is not None:
        p = pathlib.Path(p)
        p.mkdir(parents=True, exist_ok=True)
        return p

    return tmp_path_factory.mktemp("pkgs_dirs")


@pytest.fixture(params=[False])
def shared_pkgs_dirs(request) -> bool:
    """A dummy fixture to control the use of shared package dir."""
    return request.param


@pytest.fixture
def tmp_root_prefix(
    request,
    tmp_path_factory: pytest.TempPathFactory,
    tmp_clean_env: None,
    tmp_pkgs_dirs: pathlib.Path,
    shared_pkgs_dirs: bool,
) -> Generator[pathlib.Path, None, None]:
    """Change the micromamba root directory to a tmp folder for the duration of a test."""
    new_root_prefix = tmp_path_factory.mktemp("mamba")
    new_root_prefix.mkdir(parents=True, exist_ok=True)
    os.environ["MAMBA_ROOT_PREFIX"] = str(new_root_prefix)

    if shared_pkgs_dirs:
        os.environ["CONDA_PKGS_DIRS"] = str(tmp_pkgs_dirs)

    yield new_root_prefix

    # Pytest would clean it automatically but this can be large (0.5 Gb for repodata)
    # We clean it explicitly
    if not request.config.getoption("--no-eager-clean"):
        if new_root_prefix.exists():
            helpers.rmtree(new_root_prefix)
    # os.environ restored by tmp_clean_env and tmp_environ


@pytest.fixture(params=[helpers.random_string])
def tmp_env_name(request) -> str:
    """Return the explicit or implicit parametrization."""
    if callable(request.param):
        return request.param()
    return request.param


@pytest.fixture
def tmp_empty_env(
    tmp_root_prefix: pathlib.Path, tmp_env_name: str
) -> Generator[pathlib.Path, None, None]:
    """An empty environment created under a temporary root prefix."""
    helpers.create("-n", tmp_env_name, no_dry_run=True)
    yield tmp_root_prefix / "envs" / tmp_env_name


@pytest.fixture
def tmp_prefix(tmp_empty_env: pathlib.Path) -> Generator[pathlib.Path, None, None]:
    """Change the conda prefix to a tmp folder for the duration of a test."""
    os.environ["CONDA_PREFIX"] = str(tmp_empty_env)
    yield tmp_empty_env
    # os.environ restored by tmp_environ through tmp_root_prefix


@pytest.fixture
def tmp_xtensor_env(tmp_prefix: pathlib.Path) -> Generator[pathlib.Path, None, None]:
    """An activated environment with Xtensor installed."""
    helpers.install("-c", "conda-forge", "--json", "xtensor", no_dry_run=True)
    yield tmp_prefix
