import errno
import json
import os
import platform
import random
import re
import shutil
import string
import subprocess
from enum import Enum
from pathlib import Path

import pytest
import yaml


def subprocess_run(*args: str, **kwargs) -> str:
    """Execute a command in a subprocess while properly capturing stderr in exceptions."""
    try:
        p = subprocess.run(args, capture_output=True, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        print(f"Command {args} failed with stderr: {e.stderr.decode()}")
        print(f"Command {args} failed with stdout: {e.stdout.decode()}")
        raise e
    return p.stdout


class DryRun(Enum):
    OFF = "OFF"
    DRY = "DRY"
    ULTRA_DRY = "ULTRA_DRY"


use_offline = False
channel = ["-c", "conda-forge"]
dry_run_tests = DryRun(
    os.environ["MAMBA_DRY_RUN_TESTS"] if ("MAMBA_DRY_RUN_TESTS" in os.environ) else "OFF"
)

MAMBA_NO_PREFIX_CHECK = 1 << 0
MAMBA_ALLOW_EXISTING_PREFIX = 1 << 1
MAMBA_ALLOW_MISSING_PREFIX = 1 << 2
MAMBA_ALLOW_NOT_ENV_PREFIX = 1 << 3
MAMBA_EXPECT_EXISTING_PREFIX = 1 << 4

MAMBA_NOT_ALLOW_EXISTING_PREFIX = 0
MAMBA_NOT_ALLOW_MISSING_PREFIX = 0
MAMBA_NOT_ALLOW_NOT_ENV_PREFIX = 0
MAMBA_NOT_EXPECT_EXISTING_PREFIX = 0


def lib_prefix() -> Path:
    """A potential prefix used for library in Conda environments."""
    if platform.system() == "Windows":
        return Path("Library")
    return Path("")


def get_umamba(cwd=os.getcwd()):
    if os.getenv("TEST_MAMBA_EXE"):
        umamba = os.getenv("TEST_MAMBA_EXE")
    else:
        raise RuntimeError("Mamba/Micromamba not found! Set TEST_MAMBA_EXE env variable")
    return umamba


def random_string(n: int = 10) -> str:
    """Return random characters and digits."""
    return "".join(random.choices(string.ascii_uppercase + string.digits, k=n))


def remove_whitespaces(s: str) -> str:
    """Return the input string with extra whitespaces removed."""
    return re.sub(r"\s+", " ", s).strip()


def shell(*args, cwd=os.getcwd(), **kwargs):
    umamba = get_umamba(cwd=cwd)
    cmd = [umamba, "shell"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]

    res = subprocess_run(*cmd, **kwargs)
    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except json.decoder.JSONDecodeError as e:
            print(f"Error when loading JSON output from {res}")
            raise (e)
    if "--print-config-only" in args:
        return yaml.load(res, Loader=yaml.FullLoader)
    return res.decode()


def info(*args, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "info"] + [arg for arg in args if arg]
    res = subprocess_run(*cmd, **kwargs)
    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except json.decoder.JSONDecodeError as e:
            print(f"Error when loading JSON output from {res}")
            raise (e)
    return res.decode()


def login(*args, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "auth", "login"] + [arg for arg in args if arg]
    res = subprocess_run(*cmd, **kwargs)
    return res.decode()


def logout(*args, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "auth", "logout"] + [arg for arg in args if arg]
    res = subprocess_run(*cmd, **kwargs)
    return res.decode()


def install(*args, default_channel=True, no_rc=True, no_dry_run=False, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "install", "-y"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]
    if default_channel:
        cmd += channel
    if no_rc:
        cmd += ["--no-rc"]
    if use_offline:
        cmd += ["--offline"]
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]
    cmd += ["--log-level=info"]

    res = subprocess_run(*cmd, **kwargs)

    if "--json" in args:
        try:
            j = json.loads(res)
            return j
        except Exception:
            print(res.decode())
            return
    if "--print-config-only" in args:
        return yaml.load(res, Loader=yaml.FullLoader)
    return res.decode()


def create(
    *args,
    default_channel=True,
    no_rc=True,
    no_dry_run=False,
    always_yes=True,
    create_cmd="create",
    **kwargs,
):
    umamba = get_umamba()
    cmd = [umamba] + create_cmd.split() + [str(arg) for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]
    if always_yes:
        cmd += ["-y"]
    if default_channel:
        cmd += channel
    if no_rc:
        cmd += ["--no-rc"]
    if use_offline:
        cmd += ["--offline"]
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    try:
        res = subprocess_run(*cmd, **kwargs)
        if "--json" in args:
            j = json.loads(res)
            return j
        if "--print-config-only" in args:
            return yaml.load(res, Loader=yaml.FullLoader)
        return res.decode()
    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def remove(*args, no_dry_run=False, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "remove", "-y"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    try:
        res = subprocess_run(*cmd, **kwargs)
        if "--json" in args:
            j = json.loads(res)
            return j
        if "--print-config-only" in args:
            return yaml.load(res, Loader=yaml.FullLoader)
        return res.decode()
    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def uninstall(*args, no_dry_run=False, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "uninstall", "-y"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    try:
        res = subprocess_run(*cmd, **kwargs)
        if "--json" in args:
            j = json.loads(res)
            return j
        if "--print-config-only" in args:
            return yaml.load(res, Loader=yaml.FullLoader)
        return res.decode()
    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def clean(*args, no_dry_run=False, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "clean", "-y"] + [arg for arg in args if arg]

    if "--print-config-only" in args:
        cmd += ["--debug"]
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    try:
        res = subprocess.check_output(cmd, **kwargs)
        if "--json" in args:
            j = json.loads(res)
            return j
        if "--print-config-only" in args:
            return yaml.load(res, Loader=yaml.FullLoader)
        return res.decode()
    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def update(*args, default_channel=True, no_rc=True, no_dry_run=False, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "update", "-y"] + [arg for arg in args if arg]
    if use_offline:
        cmd += ["--offline"]
    if no_rc:
        cmd += ["--no-rc"]
    if default_channel:
        cmd += channel
    if (dry_run_tests == DryRun.DRY) and "--dry-run" not in args and not no_dry_run:
        cmd += ["--dry-run"]

    try:
        res = subprocess_run(*cmd, **kwargs)
        if "--json" in args:
            try:
                j = json.loads(res)
                return j
            except json.decoder.JSONDecodeError as e:
                print(f"Error when loading JSON output from {res}")
                raise (e)

        return res.decode()
    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


def run_env(*args, f=None, **kwargs):
    umamba = get_umamba()
    cmd = [umamba, "env"] + [str(arg) for arg in args if arg]

    res = subprocess_run(*cmd, **kwargs)

    if "--json" in args:
        j = json.loads(res)
        return j

    return res.decode()


def umamba_list(*args, **kwargs):
    umamba = get_umamba()

    cmd = [umamba, "list"] + [str(arg) for arg in args if arg]
    res = subprocess_run(*cmd, **kwargs)

    if "--json" in args:
        j = json.loads(res)
        return j

    return res.decode()


def umamba_run(*args, **kwargs):
    umamba = get_umamba()

    cmd = [umamba, "run"] + [str(arg) for arg in args if arg]
    res = subprocess_run(*cmd, **kwargs)

    if "--json" in args:
        j = json.loads(res)
        return j

    return res.decode()


def umamba_repoquery(*args, no_rc=True, **kwargs):
    umamba = get_umamba()

    cmd = [umamba, "repoquery"] + [str(arg) for arg in args if arg]

    if no_rc:
        cmd += ["--no-rc"]

    res = subprocess_run(*cmd, **kwargs)

    if "--json" in args:
        j = json.loads(res)
        return j

    return res.decode()


def get_concrete_pkg(t, needle):
    pkgs = t["actions"]["LINK"]
    for p in pkgs:
        if p["name"] == needle:
            return f"{p['name']}-{p['version']}-{p['build_string']}"
    raise RuntimeError("Package not found in transaction")


def get_env(n, f=None):
    root_prefix = os.getenv("MAMBA_ROOT_PREFIX")
    if f:
        return Path(os.path.join(root_prefix, "envs", n, f))
    else:
        return Path(os.path.join(root_prefix, "envs", n))


def get_pkg(n, f=None, root_prefix=None):
    if not root_prefix:
        root_prefix = os.getenv("MAMBA_ROOT_PREFIX")
    if f:
        return Path(os.path.join(root_prefix, "pkgs", n, f))
    else:
        return Path(os.path.join(root_prefix, "pkgs", n))


def get_concrete_pkg_info(env, pkg_name):
    with open(os.path.join(env, "conda-meta", pkg_name + ".json")) as fi:
        return json.load(fi)


def read_windows_registry(target_path):  # pragma: no cover
    import winreg

    # HKEY_LOCAL_MACHINE\Software\Microsoft\Command Processor\AutoRun
    # HKEY_CURRENT_USER\Software\Microsoft\Command Processor\AutoRun
    # returns value_value, value_type  -or-  None, None if target does not exist
    main_key, the_rest = target_path.split("\\", 1)
    subkey_str, value_name = the_rest.rsplit("\\", 1)
    main_key = getattr(winreg, main_key)

    try:
        key = winreg.OpenKey(main_key, subkey_str, 0, winreg.KEY_READ)
    except OSError as e:
        if e.errno != errno.ENOENT:
            raise
        return None, None

    try:
        value_tuple = winreg.QueryValueEx(key, value_name)
        value_value = value_tuple[0]
        if isinstance(value_value, str):
            value_value = value_value.strip()
        value_type = value_tuple[1]
        return value_value, value_type
    except Exception:
        # [WinError 2] The system cannot find the file specified
        winreg.CloseKey(key)
        return None, None
    finally:
        winreg.CloseKey(key)


def write_windows_registry(target_path, value_value, value_type):  # pragma: no cover
    import winreg

    main_key, the_rest = target_path.split("\\", 1)
    subkey_str, value_name = the_rest.rsplit("\\", 1)
    main_key = getattr(winreg, main_key)
    try:
        key = winreg.OpenKey(main_key, subkey_str, 0, winreg.KEY_WRITE)
    except OSError as e:
        if e.errno != errno.ENOENT:
            raise
        key = winreg.CreateKey(main_key, subkey_str)
    try:
        winreg.SetValueEx(key, value_name, 0, value_type, value_value)
    finally:
        winreg.CloseKey(key)


@pytest.fixture(scope="session")
def cache_warming():
    cache = Path(os.path.expanduser(os.path.join("~", "cache" + random_string())))
    os.makedirs(cache)

    os.environ["CONDA_PKGS_DIRS"] = str(cache)
    tmp_prefix = os.path.expanduser(os.path.join("~", "tmpprefix" + random_string()))

    res = create("-p", tmp_prefix, "xtensor", "--json", no_dry_run=True)
    pkg_name = get_concrete_pkg(res, "xtensor")

    yield cache, pkg_name

    if "CONDA_PKGS_DIRS" in os.environ:
        os.environ.pop("CONDA_PKGS_DIRS")
    rmtree(cache)
    rmtree(tmp_prefix)


@pytest.fixture(scope="session")
def existing_cache(cache_warming):
    yield cache_warming[0]


@pytest.fixture(scope="session")
def repodata_files(existing_cache):
    yield [f for f in existing_cache.iterdir() if f.is_file() and f.suffix == ".json"]


@pytest.fixture(scope="session")
def test_pkg(cache_warming):
    yield cache_warming[1]


@pytest.fixture
def first_cache_is_writable():
    return True


def link_dir(new_dir, existing_dir, prefixes=None):
    for i in existing_dir.iterdir():
        if i.is_dir():
            subdir = new_dir / i.name
            os.makedirs(subdir, exist_ok=True)
            link_dir(subdir, i)
        elif i.is_symlink():
            linkto = os.readlink(i)
            os.symlink(linkto, new_dir / i.name)
        elif i.is_file():
            os.makedirs(new_dir, exist_ok=True)
            name = i.name
            os.link(i, new_dir / name)


def recursive_chmod(path: Path, permission, is_root=True):
    p = Path(path)

    if not p.is_symlink():
        os.chmod(p, permission)

    if p.is_dir():
        for i in p.iterdir():
            recursive_chmod(i, permission, is_root=False)


def rmtree(path: Path):
    path = Path(path)

    if not path.exists():
        return

    recursive_chmod(path, 0o700)

    def handleError(func, p, exc_info):
        recursive_chmod(p, 0o700)
        func(p)

    if path.is_dir():
        shutil.rmtree(path, onerror=handleError)
    else:
        os.remove(path)


def get_fake_activate(prefix):
    prefix = Path(prefix)
    env = os.environ.copy()
    curpath = env["PATH"]
    curpath = curpath.split(os.pathsep)
    if platform.system() == "Windows":
        addpath = [
            prefix,
            prefix / "Library" / "mingw-w64" / "bin",
            prefix / "Library" / "usr" / "bin",
            prefix / "Library" / "bin",
            prefix / "Scripts",
            prefix / "bin",
        ]
    else:
        addpath = [prefix / "bin"]
    env["PATH"] = os.pathsep.join([str(x) for x in addpath + curpath])
    env["CONDA_PREFIX"] = str(prefix)
    return env


def create_with_chan_pkg(env_name, channels, package):
    cmd = [
        "-n",
        env_name,
        "--override-channels",
        "--strict-channel-priority",
        "--dry-run",
        "--json",
    ]
    for channel in channels:
        cmd += ["-c", os.path.abspath(os.path.join(*channel))]
    cmd.append(package)

    return create(*cmd, default_channel=False, no_rc=False)


class PackageChecker:
    # Provides integrity checking operations for an installed package, based on its manifest.

    package_name: string
    install_prefix_root_dir: Path
    manifests_dir: Path

    _manifest_info: object
    _manifest_json_path: Path

    def __init__(
        self, package_name: string, install_prefix_root_dir: Path, require_manifest: bool = True
    ):
        # package_name : the name of the package to work with, without version or build name, for example 'xtensor'
        # install_prefix_root_dir : the absolute path to the directory in which the package should be installed and found

        self._require_manifest = require_manifest

        assert install_prefix_root_dir.is_absolute()
        assert package_name
        self.package_name = package_name

        assert install_prefix_root_dir
        assert os.path.isdir(install_prefix_root_dir), (
            f"not a directory or doesnt exist: {install_prefix_root_dir}"
        )
        self.install_prefix_root_dir = install_prefix_root_dir

        if require_manifest:
            self.manifests_dir = self.install_prefix_root_dir / "conda-meta"
            assert os.path.isdir(self.manifests_dir), (
                f"not a directory or doesnt exist: {self.manifests_dir}"
            )

    def check_install_integrity(self):
        # Checks that the manifest of the package is installed and checks that every file listed in it
        # exists. An assertion will fail otherwise.
        manifest_info = self.get_manifest_info()

        for file in manifest_info["files"]:
            installed_file_path = self.install_prefix_root_dir.joinpath(file)
            assert installed_file_path.is_file()

    def get_manifest_info(self) -> object:
        # Look for and read the manifest file for the package and returns a dict with its content.
        # If the manifest file is not found or if opening it fails, an assertion will fail.
        if not hasattr(self, "_manifest_info") or not self._manifest_info:
            manifest_json_paths = list(self.manifests_dir.glob(f"{self.package_name}-*.*.*-*.json"))
            assert manifest_json_paths
            assert len(manifest_json_paths) == 1

            manifest_json_path = manifest_json_paths[0]
            with open(manifest_json_path) as json_file:
                self._manifest_info = json.load(json_file)

        assert self._manifest_info
        return self._manifest_info

    def find_installed(self, name_or_relative_path: string) -> Path:
        # Searches in the manifest of the package a given file name or relative path that must have been installed.
        # Returns the absolute path to that file once found, or None if not found.
        # An assertion will fail if the file is found in the manifst but does not exist in the install directory.

        if self._require_manifest:
            manifest_info = self.get_manifest_info()

            for file in manifest_info["files"]:
                if file.endswith(name_or_relative_path):
                    absolute_path = self.install_prefix_root_dir.joinpath(file).absolute()
                    assert absolute_path.exists()
                    return absolute_path
        else:
            # We search for the file in the directory, without assuming it must exist in the manifest
            for file in self.install_prefix_root_dir.glob(f"**/{name_or_relative_path}"):
                return file.absolute()

        return None

    def get_name_version_build(self) -> string:
        # A name that matches what `get_concrete_pkg` would return: `package_name-X.Y.Z-build_number``
        manifest_info = self.get_manifest_info()
        return f"{manifest_info['name']}-{manifest_info['version']}-{manifest_info['build_string']}"


def assert_state_file(state_file_path: Path, expected_state: dict):
    """Assert that the state file contains the expected fields.

    Args:
        state_file_path: Path to the state file (conda-meta/state)
        expected_state: Dictionary of expected fields, including 'env_vars' and any other fields
    """
    assert state_file_path.exists(), f"State file does not exist: {state_file_path}"
    with open(state_file_path) as f:
        state = json.loads(f.read())
    for field_name, expected_value in expected_state.items():
        assert field_name in state, f"Field '{field_name}' not found in state file"
        assert state[field_name] == expected_value, (
            f"Expected {field_name} to be {expected_value}, but got {state[field_name]}"
        )
