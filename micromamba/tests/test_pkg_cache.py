import os
import platform
import shutil
import subprocess
import glob
from pathlib import Path
from typing import Optional

import pytest

from . import helpers

package_to_check = "xtensor"
package_version_to_check = "0.27.0"
file_to_find_in_package = "xtensor.hpp"


def package_to_check_requirements():
    return f"{package_to_check}={package_version_to_check}"


def find_cache_archive(cache: Path, pkg_name: str) -> Optional[Path]:
    """Find the archive used in cache from the complete build name."""
    tar_bz2 = cache / f"{pkg_name}.tar.bz2"
    conda = cache / f"{pkg_name}.conda"
    if tar_bz2.exists():
        return tar_bz2
    elif conda.exists():
        return conda
    return None


def find_pkg_build(cache: Path, name: str) -> str:
    """Find the build name of a package in the cache from the package name.

    Returns the relative path from cache root (e.g., "package-name" or
    "https/.../platform/package-name") to support nested cache structure.
    """
    # Search recursively for package directories
    # Filter to only match package directories (not subdirectories within packages)
    # Package directories are direct children of platform directories (e.g., linux-64) or cache root
    all_matches = [p for p in cache.rglob(f"{name}*") if p.is_dir()]
    matches = []
    for p in all_matches:
        parent = p.parent
        # Package directories are at platform level or cache root, not nested deeper
        # Check if parent is a platform directory (contains platform identifiers)
        is_platform_dir = any(
            platform in parent.name.lower() for platform in ["linux", "win", "osx", "noarch"]
        ) or parent.name.endswith(("64", "32", "arm64", "armv7"))
        is_at_cache_root = parent == cache

        # Package build names have format: name-version-build (e.g., xtensor-0.27.0-hb700be7_0)
        # They contain at least 2 dashes (name-version-build)
        looks_like_package_build = p.name.count("-") >= 2

        if (is_platform_dir or is_at_cache_root) and looks_like_package_build:
            matches.append(p)

    if len(matches) == 0:
        # List all top-level directories in cache for debugging
        all_dirs = [p.name for p in cache.iterdir() if p.is_dir()]
        raise AssertionError(
            f"Could not find package directory matching '{name}*' in cache {cache}. "
            f"Found top-level directories: {all_dirs}"
        )
    assert len(matches) == 1, (
        f"Found {len(matches)} directories matching '{name}*' in cache {cache}, "
        f"expected exactly 1. Found: {[str(m) for m in matches]}"
    )
    # Return relative path from cache root to support nested structures
    try:
        return str(matches[0].relative_to(cache))
    except ValueError:
        # If not relative (shouldn't happen), return just the name
        return matches[0].name


@pytest.fixture(scope="module")
def tmp_shared_cache_test_pkg(tmp_path_factory: pytest.TempPathFactory):
    """Create shared cache folder with a test package."""
    root = tmp_path_factory.mktemp(package_to_check)
    helpers.create(
        "-n",
        package_to_check,
        "--no-env",
        "--no-rc",
        "-r",
        root,
        "-c",
        "conda-forge",
        "--no-deps",
        package_to_check_requirements(),
        no_dry_run=True,
    )
    return root / "pkgs"


@pytest.fixture(params=[True])
def tmp_cache_writable(request) -> bool:
    """A dummy fixture to control the writability of ``tmp_cache``."""
    return request.param


@pytest.fixture
def tmp_cache(
    tmp_root_prefix: Path, tmp_shared_cache_test_pkg: Path, tmp_cache_writable: bool
) -> Path:
    """The default cache folder associated with the root_prefix and a test package."""
    cache: Path = tmp_root_prefix / "pkgs"
    shutil.copytree(tmp_shared_cache_test_pkg, cache, dirs_exist_ok=True)
    if not tmp_cache_writable:
        helpers.recursive_chmod(cache, 0o500)
    return cache


@pytest.fixture
def tmp_cache_test_package_dir(tmp_cache: Path) -> Path:
    """The location of the package-t-test's cache directory within the package cache."""
    return tmp_cache / find_pkg_build(tmp_cache, package_to_check)


@pytest.fixture
def tmp_cache_test_pkg(tmp_cache: Path) -> Path:
    """The location of the package-to-test's cache artifact (tarball) within the cache directory."""
    return find_cache_archive(tmp_cache, find_pkg_build(tmp_cache, package_to_check))


@pytest.fixture
def tmp_cache_file_in_test_package(tmp_cache_test_package_dir: Path) -> Path:
    """The location of the file in the package to test within the cache directory."""
    pkg_checker = helpers.PackageChecker(
        package_to_check, tmp_cache_test_package_dir, require_manifest=False
    )
    return pkg_checker.find_installed(file_to_find_in_package)


class TestPkgCache:
    def test_extracted_file_deleted(
        self, tmp_home, tmp_cache_file_in_test_package, tmp_root_prefix
    ):
        old_ino = tmp_cache_file_in_test_package.stat().st_ino
        os.remove(tmp_cache_file_in_test_package)

        env_name = "some_env"
        helpers.create(package_to_check_requirements(), "-n", env_name, no_dry_run=True)

        env_dir = tmp_root_prefix / "envs" / env_name
        pkg_checker = helpers.PackageChecker(package_to_check, env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tmp_cache_file_in_test_package.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_file_in_test_package.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    @pytest.mark.parametrize("safety_checks", ["disabled", "warn", "enabled"])
    def test_extracted_file_corrupted(
        self, tmp_home, tmp_root_prefix, tmp_cache_file_in_test_package, safety_checks
    ):
        old_ino = tmp_cache_file_in_test_package.stat().st_ino

        with open(tmp_cache_file_in_test_package, "w") as f:
            f.write("//corruption")

        env_name = "x1"
        helpers.create(
            package_to_check_requirements(),
            "-n",
            env_name,
            "--json",
            "--safety-checks",
            safety_checks,
            no_dry_run=True,
        )

        env_dir = tmp_root_prefix / "envs" / env_name
        pkg_checker = helpers.PackageChecker(package_to_check, env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tmp_cache_file_in_test_package.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_file_in_test_package.stat().st_ino == linked_file_stats.st_ino

        if safety_checks == "enabled":
            assert old_ino != linked_file_stats.st_ino
        else:
            assert old_ino == linked_file_stats.st_ino

    def test_tarball_deleted(
        self,
        tmp_home,
        tmp_root_prefix,
        tmp_cache_test_pkg,
        tmp_cache_file_in_test_package,
        tmp_cache,
    ):
        assert tmp_cache_test_pkg.exists()
        os.remove(tmp_cache_test_pkg)

        env_name = "x1"
        helpers.create(package_to_check_requirements(), "-n", env_name, "--json", no_dry_run=True)

        env_dir = tmp_root_prefix / "envs" / env_name
        pkg_checker = helpers.PackageChecker(package_to_check, env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert not tmp_cache_test_pkg.exists()
        assert tmp_cache_file_in_test_package.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_file_in_test_package.stat().st_ino == linked_file_stats.st_ino

    def test_tarball_and_extracted_file_deleted(
        self, tmp_home, tmp_root_prefix, tmp_cache_test_pkg, tmp_cache_file_in_test_package
    ):
        test_pkg_size = tmp_cache_test_pkg.stat().st_size
        old_ino = tmp_cache_file_in_test_package.stat().st_ino
        os.remove(tmp_cache_file_in_test_package)
        os.remove(tmp_cache_test_pkg)

        env_name = "x1"
        helpers.create(package_to_check_requirements(), "-n", env_name, "--json", no_dry_run=True)

        env_dir = tmp_root_prefix / "envs" / env_name
        pkg_checker = helpers.PackageChecker(package_to_check, env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tmp_cache_test_pkg.exists()
        assert test_pkg_size == tmp_cache_test_pkg.stat().st_size
        assert tmp_cache_file_in_test_package.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_file_in_test_package.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    def test_tarball_corrupted_and_extracted_file_deleted(
        self, tmp_home, tmp_root_prefix, tmp_cache_test_pkg, tmp_cache_file_in_test_package
    ):
        test_pkg_size = tmp_cache_test_pkg.stat().st_size
        old_ino = tmp_cache_file_in_test_package.stat().st_ino
        os.remove(tmp_cache_file_in_test_package)
        os.remove(tmp_cache_test_pkg)
        with open(tmp_cache_test_pkg, "w") as f:
            f.write("")

        env_name = "x1"
        helpers.create(package_to_check_requirements(), "-n", env_name, "--json", no_dry_run=True)

        env_dir = tmp_root_prefix / "envs" / env_name
        pkg_checker = helpers.PackageChecker(package_to_check, env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tmp_cache_test_pkg.exists()
        assert test_pkg_size == tmp_cache_test_pkg.stat().st_size
        assert tmp_cache_file_in_test_package.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_file_in_test_package.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    @pytest.mark.parametrize("safety_checks", ("disabled", "warn", "enabled"))
    def test_extracted_file_corrupted_no_perm(
        self,
        tmp_home,
        tmp_root_prefix,
        tmp_cache_test_pkg,
        tmp_cache_file_in_test_package,
        safety_checks,
    ):
        with open(tmp_cache_file_in_test_package, "w") as f:
            f.write("//corruption")
        helpers.recursive_chmod(tmp_cache_test_pkg, 0o500)
        #  old_ino = tmp_cache_file_in_test_package.stat().st_ino

        env = "x1"
        cmd_args = (
            package_to_check_requirements(),
            "-n",
            "--safety-checks",
            safety_checks,
            env,
            "--json",
            "-vv",
        )

        with pytest.raises(subprocess.CalledProcessError):
            helpers.create(*cmd_args, no_dry_run=True)


@pytest.fixture
def tmp_cache_alt(tmp_root_prefix: Path, tmp_shared_cache_test_pkg: Path) -> Path:
    """Make an alternative package cache outside the root prefix."""
    cache = tmp_root_prefix / "more-pkgs"  # Creating under root prefix to leverage eager cleanup
    shutil.copytree(tmp_shared_cache_test_pkg, cache, dirs_exist_ok=True)
    return cache


def repodata_json(cache: Path) -> set[Path]:
    return set((cache / "cache").glob("*.json")) - set((cache / "cache").glob("*.state.json"))


def repodata_solv(cache: Path) -> set[Path]:
    return set((cache / "cache").glob("*.solv"))


def same_repodata_json_solv(cache: Path):
    return {p.stem for p in repodata_json(cache)} == {p.stem for p in repodata_solv(cache)}


class TestMultiplePkgCaches:
    @pytest.mark.parametrize("which_cache", ["tmp_cache", "tmp_cache_alt"])
    def test_different_caches(
        self,
        tmp_home,
        tmp_root_prefix,
        tmp_cache,
        tmp_cache_alt,
        which_cache,
        monkeypatch,
    ):
        # Test parametrization
        cache = {"tmp_cache": tmp_cache, "tmp_cache_alt": tmp_cache_alt}[which_cache]

        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{cache}")
        env_name = "some_env"
        res = helpers.create(
            "-n", env_name, package_to_check_requirements(), "-v", "--json", no_dry_run=True
        )

        assert res["success"]

        env_dir = tmp_root_prefix / "envs" / env_name
        pkg_checker = helpers.PackageChecker(package_to_check, env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()

        installed_file_rel_path = linked_file.relative_to(env_dir)
        # Use find_pkg_build to get the actual package directory path (handles nested cache structure)
        pkg_dir_rel_path = find_pkg_build(cache, package_to_check)
        cache_file = cache / pkg_dir_rel_path / installed_file_rel_path

        assert cache_file.exists()

        assert linked_file.stat().st_dev == cache_file.stat().st_dev
        assert linked_file.stat().st_ino == cache_file.stat().st_ino

    @pytest.mark.parametrize("tmp_cache_writable", [False, True], indirect=True)
    def test_first_writable(
        self,
        tmp_home,
        tmp_root_prefix,
        tmp_cache_writable,
        tmp_cache,
        tmp_cache_alt,
        monkeypatch,
    ):
        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{tmp_cache},{tmp_cache_alt}")

        env_name = "some_env"
        res = helpers.create(
            "-n", env_name, package_to_check_requirements(), "--json", no_dry_run=True
        )

        assert res["success"]

        env_dir = tmp_root_prefix / "envs" / env_name
        pkg_checker = helpers.PackageChecker(package_to_check, env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()

        installed_file_rel_path = linked_file.relative_to(env_dir)
        # A previous version of this test was attempting to test that the installed file
        # was linked from the first writable pkgs dir, however it passed only because of a bug
        # in how it used pytest.
        # The first pkgs dir can be used to link, even if it is not writable.
        # Use find_pkg_build to get the actual package directory path (handles nested cache structure)
        pkg_dir_rel_path = find_pkg_build(tmp_cache, package_to_check)
        cache_file = tmp_cache / pkg_dir_rel_path / installed_file_rel_path

        assert cache_file.exists()

        assert linked_file.stat().st_dev == cache_file.stat().st_dev
        assert linked_file.stat().st_ino == cache_file.stat().st_ino

    def test_no_writable(self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt, monkeypatch):
        helpers.rmtree(tmp_cache / find_pkg_build(tmp_cache, package_to_check))
        helpers.recursive_chmod(tmp_cache, 0o500)

        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{tmp_cache},{tmp_cache_alt}")

        helpers.create("-n", "myenv", package_to_check_requirements(), "--json", no_dry_run=True)

    def test_no_writable_extracted_dir_corrupted(
        self, tmp_home, tmp_root_prefix, tmp_cache, monkeypatch
    ):
        old_cache_dir = tmp_cache / find_pkg_build(tmp_cache, package_to_check)
        if old_cache_dir.is_dir():
            files = glob.glob(
                f"**{file_to_find_in_package}", recursive=True, root_dir=old_cache_dir
            )
            for file in files:
                (old_cache_dir / file).unlink()
        helpers.recursive_chmod(tmp_cache, 0o500)

        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{tmp_cache}")

        # Mamba now handles corrupted extracted directories in read-only caches gracefully
        # by extracting to a temporary location, so the operation should succeed
        helpers.create(
            "-n", "myenv", package_to_check_requirements(), "-vv", "--json", no_dry_run=True
        )

    def test_first_writable_extracted_dir_corrupted(
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt, monkeypatch
    ):
        test_pkg_bld = find_pkg_build(tmp_cache, package_to_check)
        helpers.rmtree(tmp_cache)  # convenience for cache teardown
        os.makedirs(tmp_cache)
        open(tmp_cache / "urls.txt", "w")  # chmod only set read-only flag on Windows
        helpers.recursive_chmod(tmp_cache, 0o500)

        tmp_cache_alt_pkg_dir = tmp_cache_alt / test_pkg_bld
        if tmp_cache_alt_pkg_dir.is_dir():
            files = tmp_cache_alt_pkg_dir.glob(f"**/{file_to_find_in_package}")
            for file in files:
                helpers.rmtree(file)

        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{tmp_cache},{tmp_cache_alt}")
        env_name = "myenv"

        helpers.create(
            "-n", env_name, package_to_check_requirements(), "-vv", "--json", no_dry_run=True
        )

        install_env_dir = helpers.get_env(env_name)
        pkg_checker = helpers.PackageChecker(package_to_check, install_env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()

        # check repodata files
        assert repodata_json(tmp_cache) == set()
        assert repodata_json(tmp_cache_alt) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache_alt)

        # check tarballs
        assert find_cache_archive(tmp_cache, test_pkg_bld) is None
        assert find_cache_archive(tmp_cache_alt, test_pkg_bld).exists()

        linked_file_rel_path = linked_file.relative_to(install_env_dir)
        non_writable_cache_file = tmp_cache / test_pkg_bld / linked_file_rel_path
        writable_cache_file = tmp_cache_alt / test_pkg_bld / linked_file_rel_path

        # check extracted files
        assert not non_writable_cache_file.exists()
        assert writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == writable_cache_file.stat().st_ino

    def test_extracted_tarball_only_in_non_writable_cache(
        self,
        tmp_root_prefix,
        tmp_home,
        tmp_cache,
        tmp_cache_alt,
        tmp_cache_test_package_dir,
        monkeypatch,
    ):
        test_pkg_bld = find_pkg_build(tmp_cache, package_to_check)
        helpers.rmtree(find_cache_archive(tmp_cache, test_pkg_bld))
        helpers.rmtree(tmp_cache_alt)
        helpers.recursive_chmod(tmp_cache, 0o500)

        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{tmp_cache},{tmp_cache_alt}")
        env_name = "myenv"

        helpers.create("-n", env_name, package_to_check_requirements(), "--json", no_dry_run=True)

        install_env_dir = helpers.get_env(env_name)
        pkg_checker = helpers.PackageChecker(package_to_check, install_env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()

        # check repodata files
        assert repodata_json(tmp_cache) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache)
        assert repodata_json(tmp_cache_alt) == set()

        # check tarballs
        assert find_cache_archive(tmp_cache, test_pkg_bld) is None
        assert find_cache_archive(tmp_cache_alt, test_pkg_bld) is None

        linked_file_rel_path = linked_file.relative_to(install_env_dir)
        non_writable_cache_file = tmp_cache / test_pkg_bld / linked_file_rel_path
        writable_cache_file = tmp_cache_alt / test_pkg_bld / linked_file_rel_path

        # check extracted files
        assert non_writable_cache_file.exists()
        assert not writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == non_writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == non_writable_cache_file.stat().st_ino

    def test_missing_extracted_dir_in_non_writable_cache(
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt, monkeypatch
    ):
        test_pkg_bld = find_pkg_build(tmp_cache, package_to_check)
        helpers.rmtree(tmp_cache / test_pkg_bld)
        helpers.rmtree(tmp_cache_alt)
        helpers.recursive_chmod(tmp_cache, 0o500)

        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{tmp_cache},{tmp_cache_alt}")
        env_name = "myenv"

        helpers.create("-n", env_name, package_to_check_requirements(), "--json", no_dry_run=True)

        install_env_dir = helpers.get_env(env_name)
        pkg_checker = helpers.PackageChecker(package_to_check, install_env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()

        linked_file_rel_path = linked_file.relative_to(install_env_dir)
        non_writable_cache_file = tmp_cache / test_pkg_bld / linked_file_rel_path
        writable_cache_file = tmp_cache_alt / test_pkg_bld / linked_file_rel_path

        # check repodata files
        assert repodata_json(tmp_cache) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache)
        assert repodata_json(tmp_cache_alt) == set()

        # check tarballs
        assert find_cache_archive(tmp_cache, test_pkg_bld).exists()
        assert find_cache_archive(tmp_cache_alt, test_pkg_bld) is None

        # check extracted files
        assert not non_writable_cache_file.exists()
        assert writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == writable_cache_file.stat().st_ino

    def test_corrupted_extracted_dir_in_non_writable_cache(
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt, monkeypatch
    ):
        test_pkg_bld = find_pkg_build(tmp_cache, package_to_check)
        tmp_cache_test_pkg_dir = Path(tmp_cache / test_pkg_bld)
        if tmp_cache_test_pkg_dir.is_dir():
            files = tmp_cache_test_pkg_dir.glob(f"**/{file_to_find_in_package}")
            for file in files:
                helpers.rmtree(file)

        helpers.rmtree(tmp_cache_alt)  # convenience for cache teardown
        os.makedirs(tmp_cache_alt)
        helpers.recursive_chmod(tmp_cache, 0o500)

        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{tmp_cache},{tmp_cache_alt}")
        env_name = "myenv"

        helpers.create(
            "-n", env_name, "-vv", package_to_check_requirements(), "--json", no_dry_run=True
        )

        install_env_dir = helpers.get_env(env_name)
        pkg_checker = helpers.PackageChecker(package_to_check, install_env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()

        # check repodata files
        assert repodata_json(tmp_cache) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache)
        assert repodata_json(tmp_cache_alt) == set()

        # check tarballs
        assert find_cache_archive(tmp_cache, test_pkg_bld).exists()
        assert find_cache_archive(tmp_cache_alt, test_pkg_bld) is None

        # check extracted dir
        assert (tmp_cache / test_pkg_bld).exists()
        assert (tmp_cache_alt / test_pkg_bld).exists()

        linked_file_rel_path = linked_file.relative_to(install_env_dir)
        non_writable_cache_file = tmp_cache / test_pkg_bld / linked_file_rel_path
        writable_cache_file = tmp_cache_alt / test_pkg_bld / linked_file_rel_path

        # check extracted files
        assert not non_writable_cache_file.exists()
        assert writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == writable_cache_file.stat().st_ino

    def test_expired_but_valid_repodata_in_non_writable_cache(
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt, monkeypatch
    ):
        helpers.rmtree(tmp_cache_alt)
        helpers.recursive_chmod(tmp_cache, 0o500)

        monkeypatch.setenv("CONDA_PKGS_DIRS", f"{tmp_cache},{tmp_cache_alt}")
        env_name = "myenv"
        test_pkg_bld = find_pkg_build(tmp_cache, package_to_check)

        helpers.create(
            "-n",
            env_name,
            package_to_check_requirements(),
            "-vv",
            "--json",
            "--repodata-ttl=0",
            no_dry_run=True,
        )

        install_env_dir = helpers.get_env(env_name)
        pkg_checker = helpers.PackageChecker(package_to_check, install_env_dir)
        linked_file = pkg_checker.find_installed(file_to_find_in_package)
        assert linked_file.exists()

        # check repodata files
        assert repodata_json(tmp_cache) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache)
        assert repodata_json(tmp_cache_alt) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache_alt)

        # check tarballs
        assert find_cache_archive(tmp_cache, test_pkg_bld).exists()
        assert find_cache_archive(tmp_cache_alt, test_pkg_bld) is None

        # check extracted dir
        assert (tmp_cache / test_pkg_bld).exists()
        assert not (tmp_cache_alt / test_pkg_bld).exists()

        linked_file_rel_path = linked_file.relative_to(install_env_dir)
        non_writable_cache_file = tmp_cache / test_pkg_bld / linked_file_rel_path
        writable_cache_file = tmp_cache_alt / test_pkg_bld / linked_file_rel_path

        # check extracted files
        assert non_writable_cache_file.exists()
        assert not writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == non_writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == non_writable_cache_file.stat().st_ino
