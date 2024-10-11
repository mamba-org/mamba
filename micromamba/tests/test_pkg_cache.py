import os
import platform
import shutil
import subprocess
from pathlib import Path
from typing import Optional

import pytest

from . import helpers


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
    """Find the build name of a package in the cache from the package name."""
    matches = [p for p in cache.glob(f"{name}*") if p.is_dir()]
    assert len(matches) == 1
    return matches[0].name


@pytest.fixture(scope="module")
def tmp_shared_cache_xtensor(tmp_path_factory: pytest.TempPathFactory):
    """Create shared cache folder with an xtensor package."""
    root = tmp_path_factory.mktemp("xtensor")
    helpers.create(
        "-n",
        "xtensor",
        "--no-env",
        "--no-rc",
        "-r",
        root,
        "-c",
        "conda-forge",
        "--no-deps",
        "xtensor",
        no_dry_run=True,
    )
    return root / "pkgs"


@pytest.fixture(params=[True])
def tmp_cache_writable(request) -> bool:
    """A dummy fixture to control the writability of ``tmp_cache``."""
    return request.param


@pytest.fixture
def tmp_cache(
    tmp_root_prefix: Path, tmp_shared_cache_xtensor: Path, tmp_cache_writable: bool
) -> Path:
    """The default cache folder associated with the root_prefix and an xtensor package."""
    cache: Path = tmp_root_prefix / "pkgs"
    shutil.copytree(tmp_shared_cache_xtensor, cache, dirs_exist_ok=True)
    if not tmp_cache_writable:
        helpers.recursive_chmod(cache, 0o500)
    return cache


@pytest.fixture
def tmp_cache_xtensor_dir(tmp_cache: Path) -> Path:
    """The location of the Xtensor cache directory within the package cache."""
    return tmp_cache / find_pkg_build(tmp_cache, "xtensor")


@pytest.fixture
def tmp_cache_xtensor_pkg(tmp_cache: Path) -> Path:
    """The location of the Xtensor cache artifact (tarball) within the cache directory."""
    return find_cache_archive(tmp_cache, find_pkg_build(tmp_cache, "xtensor"))


@pytest.fixture
def tmp_cache_xtensor_hpp(tmp_cache_xtensor_dir: Path) -> Path:
    """The location of the Xtensor header (part of the package) within the cache directory."""
    return tmp_cache_xtensor_dir / helpers.xtensor_hpp


class TestPkgCache:
    def test_extracted_file_deleted(self, tmp_home, tmp_cache_xtensor_hpp, tmp_root_prefix):
        old_ino = tmp_cache_xtensor_hpp.stat().st_ino
        os.remove(tmp_cache_xtensor_hpp)

        env_name = "some_env"
        helpers.create("xtensor", "-n", env_name, no_dry_run=True)

        linked_file = tmp_root_prefix / "envs" / env_name / helpers.xtensor_hpp
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tmp_cache_xtensor_hpp.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_xtensor_hpp.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    @pytest.mark.parametrize("safety_checks", ["disabled", "warn", "enabled"])
    def test_extracted_file_corrupted(
        self, tmp_home, tmp_root_prefix, tmp_cache_xtensor_hpp, safety_checks
    ):
        old_ino = tmp_cache_xtensor_hpp.stat().st_ino

        with open(tmp_cache_xtensor_hpp, "w") as f:
            f.write("//corruption")

        env_name = "x1"
        helpers.create(
            "xtensor",
            "-n",
            env_name,
            "--json",
            "--safety-checks",
            safety_checks,
            no_dry_run=True,
        )

        linked_file = tmp_root_prefix / "envs" / env_name / helpers.xtensor_hpp
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tmp_cache_xtensor_hpp.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_xtensor_hpp.stat().st_ino == linked_file_stats.st_ino

        if safety_checks == "enabled":
            assert old_ino != linked_file_stats.st_ino
        else:
            assert old_ino == linked_file_stats.st_ino

    def test_tarball_deleted(
        self,
        tmp_home,
        tmp_root_prefix,
        tmp_cache_xtensor_pkg,
        tmp_cache_xtensor_hpp,
        tmp_cache,
    ):
        assert tmp_cache_xtensor_pkg.exists()
        os.remove(tmp_cache_xtensor_pkg)

        env_name = "x1"
        helpers.create("xtensor", "-n", env_name, "--json", no_dry_run=True)

        linked_file = tmp_root_prefix / "envs" / env_name / helpers.xtensor_hpp
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert not tmp_cache_xtensor_pkg.exists()
        assert tmp_cache_xtensor_hpp.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_xtensor_hpp.stat().st_ino == linked_file_stats.st_ino

    def test_tarball_and_extracted_file_deleted(
        self, tmp_home, tmp_root_prefix, tmp_cache_xtensor_pkg, tmp_cache_xtensor_hpp
    ):
        xtensor_pkg_size = tmp_cache_xtensor_pkg.stat().st_size
        old_ino = tmp_cache_xtensor_hpp.stat().st_ino
        os.remove(tmp_cache_xtensor_hpp)
        os.remove(tmp_cache_xtensor_pkg)

        env_name = "x1"
        helpers.create("xtensor", "-n", env_name, "--json", no_dry_run=True)

        linked_file = tmp_root_prefix / "envs" / env_name / helpers.xtensor_hpp
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tmp_cache_xtensor_pkg.exists()
        assert xtensor_pkg_size == tmp_cache_xtensor_pkg.stat().st_size
        assert tmp_cache_xtensor_hpp.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_xtensor_hpp.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    def test_tarball_corrupted_and_extracted_file_deleted(
        self, tmp_home, tmp_root_prefix, tmp_cache_xtensor_pkg, tmp_cache_xtensor_hpp
    ):
        xtensor_pkg_size = tmp_cache_xtensor_pkg.stat().st_size
        old_ino = tmp_cache_xtensor_hpp.stat().st_ino
        os.remove(tmp_cache_xtensor_hpp)
        os.remove(tmp_cache_xtensor_pkg)
        with open(tmp_cache_xtensor_pkg, "w") as f:
            f.write("")

        env_name = "x1"
        helpers.create("xtensor", "-n", env_name, "--json", no_dry_run=True)

        linked_file = tmp_root_prefix / "envs" / env_name / helpers.xtensor_hpp
        assert linked_file.exists()
        linked_file_stats = linked_file.stat()

        assert tmp_cache_xtensor_pkg.exists()
        assert xtensor_pkg_size == tmp_cache_xtensor_pkg.stat().st_size
        assert tmp_cache_xtensor_hpp.stat().st_dev == linked_file_stats.st_dev
        assert tmp_cache_xtensor_hpp.stat().st_ino == linked_file_stats.st_ino
        assert old_ino != linked_file_stats.st_ino

    @pytest.mark.parametrize("safety_checks", ("disabled", "warn", "enabled"))
    def test_extracted_file_corrupted_no_perm(
        self,
        tmp_home,
        tmp_root_prefix,
        tmp_cache_xtensor_pkg,
        tmp_cache_xtensor_hpp,
        safety_checks,
    ):
        with open(tmp_cache_xtensor_hpp, "w") as f:
            f.write("//corruption")
        helpers.recursive_chmod(tmp_cache_xtensor_pkg, 0o500)
        #  old_ino = tmp_cache_xtensor_hpp.stat().st_ino

        env = "x1"
        cmd_args = (
            "xtensor",
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
def tmp_cache_alt(tmp_root_prefix: Path, tmp_shared_cache_xtensor: Path) -> Path:
    """Make an alternative package cache outside the root prefix."""
    cache = tmp_root_prefix / "more-pkgs"  # Creating under root prefix to leverage eager cleanup
    shutil.copytree(tmp_shared_cache_xtensor, cache, dirs_exist_ok=True)
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
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt, which_cache
    ):
        # Test parametrization
        cache = {"tmp_cache": tmp_cache, "tmp_cache_alt": tmp_cache_alt}[which_cache]

        os.environ["CONDA_PKGS_DIRS"] = f"{cache}"
        env_name = "some_env"
        res = helpers.create("-n", env_name, "xtensor", "-v", "--json", no_dry_run=True)

        linked_file = tmp_root_prefix / "envs" / env_name / helpers.xtensor_hpp
        assert linked_file.exists()

        pkg_name = helpers.get_concrete_pkg(res, "xtensor")
        cache_file = cache / pkg_name / helpers.xtensor_hpp

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
    ):
        os.environ["CONDA_PKGS_DIRS"] = f"{tmp_cache},{tmp_cache_alt}"

        env_name = "some_env"
        res = helpers.create("-n", env_name, "xtensor", "--json", no_dry_run=True)

        linked_file = tmp_root_prefix / "envs" / env_name / helpers.xtensor_hpp
        assert linked_file.exists()

        pkg_name = helpers.get_concrete_pkg(res, "xtensor")
        # A previous version of this test was attempting to test that the installed file
        # was linked from the first writable pkgs dir, however it passed only because of a bug
        # in how it used pytest.
        # The first pkgs dir can be used to link, even if it is not writable.
        cache_file = tmp_cache / pkg_name / helpers.xtensor_hpp

        assert cache_file.exists()

        assert linked_file.stat().st_dev == cache_file.stat().st_dev
        assert linked_file.stat().st_ino == cache_file.stat().st_ino

    def test_no_writable(self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt):
        helpers.rmtree(tmp_cache / find_pkg_build(tmp_cache, "xtensor"))
        helpers.recursive_chmod(tmp_cache, 0o500)

        os.environ["CONDA_PKGS_DIRS"] = f"{tmp_cache},{tmp_cache_alt}"

        helpers.create("-n", "myenv", "xtensor", "--json", no_dry_run=True)

    def test_no_writable_extracted_dir_corrupted(self, tmp_home, tmp_root_prefix, tmp_cache):
        (tmp_cache / find_pkg_build(tmp_cache, "xtensor") / helpers.xtensor_hpp).unlink()
        helpers.recursive_chmod(tmp_cache, 0o500)

        os.environ["CONDA_PKGS_DIRS"] = f"{tmp_cache}"

        with pytest.raises(subprocess.CalledProcessError):
            helpers.create("-n", "myenv", "xtensor", "-vv", "--json", no_dry_run=True)

    def test_first_writable_extracted_dir_corrupted(
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt
    ):
        xtensor_bld = find_pkg_build(tmp_cache, "xtensor")
        helpers.rmtree(tmp_cache)  # convenience for cache teardown
        os.makedirs(tmp_cache)
        open(tmp_cache / "urls.txt", "w")  # chmod only set read-only flag on Windows
        helpers.recursive_chmod(tmp_cache, 0o500)
        helpers.rmtree(tmp_cache_alt / xtensor_bld / helpers.xtensor_hpp)

        os.environ["CONDA_PKGS_DIRS"] = f"{tmp_cache},{tmp_cache_alt}"
        env_name = "myenv"

        helpers.create("-n", env_name, "xtensor", "-vv", "--json", no_dry_run=True)

        linked_file = helpers.get_env(env_name, helpers.xtensor_hpp)
        assert linked_file.exists()

        # check repodata files
        assert repodata_json(tmp_cache) == set()
        assert repodata_json(tmp_cache_alt) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache_alt)

        # check tarballs
        assert find_cache_archive(tmp_cache, xtensor_bld) is None
        assert find_cache_archive(tmp_cache_alt, xtensor_bld).exists()

        non_writable_cache_file = tmp_cache / xtensor_bld / helpers.xtensor_hpp
        writable_cache_file = tmp_cache_alt / xtensor_bld / helpers.xtensor_hpp

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
        tmp_cache_xtensor_dir,
    ):
        xtensor_bld = find_pkg_build(tmp_cache, "xtensor")
        helpers.rmtree(find_cache_archive(tmp_cache, xtensor_bld))
        helpers.rmtree(tmp_cache_alt)
        helpers.recursive_chmod(tmp_cache, 0o500)

        os.environ["CONDA_PKGS_DIRS"] = f"{tmp_cache},{tmp_cache_alt}"
        env_name = "myenv"

        helpers.create("-n", env_name, "xtensor", "--json", no_dry_run=True)

        linked_file = helpers.get_env(env_name, helpers.xtensor_hpp)
        assert linked_file.exists()

        # check repodata files
        assert repodata_json(tmp_cache) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache)
        assert repodata_json(tmp_cache_alt) == set()

        # check tarballs
        assert find_cache_archive(tmp_cache, xtensor_bld) is None
        assert find_cache_archive(tmp_cache_alt, xtensor_bld) is None

        non_writable_cache_file = tmp_cache / xtensor_bld / helpers.xtensor_hpp
        writable_cache_file = tmp_cache_alt / xtensor_bld / helpers.xtensor_hpp

        # check extracted files
        assert non_writable_cache_file.exists()
        assert not writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == non_writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == non_writable_cache_file.stat().st_ino

    def test_missing_extracted_dir_in_non_writable_cache(
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt
    ):
        xtensor_bld = find_pkg_build(tmp_cache, "xtensor")
        helpers.rmtree(tmp_cache / xtensor_bld)
        helpers.rmtree(tmp_cache_alt)
        helpers.recursive_chmod(tmp_cache, 0o500)

        os.environ["CONDA_PKGS_DIRS"] = f"{tmp_cache},{tmp_cache_alt}"
        env_name = "myenv"

        helpers.create("-n", env_name, "xtensor", "--json", no_dry_run=True)

        linked_file = helpers.get_env(env_name, helpers.xtensor_hpp)
        assert linked_file.exists()

        non_writable_cache_file = tmp_cache / xtensor_bld / helpers.xtensor_hpp
        writable_cache_file = tmp_cache_alt / xtensor_bld / helpers.xtensor_hpp

        # check repodata files
        assert repodata_json(tmp_cache) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache)
        assert repodata_json(tmp_cache_alt) == set()

        # check tarballs
        assert find_cache_archive(tmp_cache, xtensor_bld).exists()
        assert find_cache_archive(tmp_cache_alt, xtensor_bld) is None

        # check extracted files
        assert not non_writable_cache_file.exists()
        assert writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == writable_cache_file.stat().st_ino

    def test_corrupted_extracted_dir_in_non_writable_cache(
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt
    ):
        xtensor_bld = find_pkg_build(tmp_cache, "xtensor")
        helpers.rmtree(tmp_cache / xtensor_bld / helpers.xtensor_hpp)
        helpers.rmtree(tmp_cache_alt)  # convenience for cache teardown
        os.makedirs(tmp_cache_alt)
        helpers.recursive_chmod(tmp_cache, 0o500)

        os.environ["CONDA_PKGS_DIRS"] = f"{tmp_cache},{tmp_cache_alt}"
        env_name = "myenv"

        helpers.create("-n", env_name, "-vv", "xtensor", "--json", no_dry_run=True)

        linked_file = helpers.get_env(env_name, helpers.xtensor_hpp)
        assert linked_file.exists()

        # check repodata files
        assert repodata_json(tmp_cache) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache)
        assert repodata_json(tmp_cache_alt) == set()

        # check tarballs
        assert find_cache_archive(tmp_cache, xtensor_bld).exists()
        assert find_cache_archive(tmp_cache_alt, xtensor_bld) is None

        # check extracted dir
        assert (tmp_cache / xtensor_bld).exists()
        assert (tmp_cache_alt / xtensor_bld).exists()

        non_writable_cache_file = tmp_cache / xtensor_bld / helpers.xtensor_hpp
        writable_cache_file = tmp_cache_alt / xtensor_bld / helpers.xtensor_hpp

        # check extracted files
        assert not non_writable_cache_file.exists()
        assert writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == writable_cache_file.stat().st_ino

    def test_expired_but_valid_repodata_in_non_writable_cache(
        self, tmp_home, tmp_root_prefix, tmp_cache, tmp_cache_alt
    ):
        helpers.rmtree(tmp_cache_alt)
        helpers.recursive_chmod(tmp_cache, 0o500)

        os.environ["CONDA_PKGS_DIRS"] = f"{tmp_cache},{tmp_cache_alt}"
        env_name = "myenv"
        xtensor_bld = find_pkg_build(tmp_cache, "xtensor")

        helpers.create(
            "-n",
            env_name,
            "xtensor",
            "-vv",
            "--json",
            "--repodata-ttl=0",
            no_dry_run=True,
        )

        linked_file = helpers.get_env(env_name, helpers.xtensor_hpp)
        assert linked_file.exists()

        # check repodata files
        assert repodata_json(tmp_cache) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache)
        assert repodata_json(tmp_cache_alt) != set()
        if platform.system() != "Windows":  # No .solv on Windows
            assert same_repodata_json_solv(tmp_cache_alt)

        # check tarballs
        assert find_cache_archive(tmp_cache, xtensor_bld).exists()
        assert find_cache_archive(tmp_cache_alt, xtensor_bld) is None

        # check extracted dir
        assert (tmp_cache / xtensor_bld).exists()
        assert not (tmp_cache_alt / xtensor_bld).exists()

        non_writable_cache_file = tmp_cache / xtensor_bld / helpers.xtensor_hpp
        writable_cache_file = tmp_cache_alt / xtensor_bld / helpers.xtensor_hpp

        # check extracted files
        assert non_writable_cache_file.exists()
        assert not writable_cache_file.exists()

        # check linked files
        assert linked_file.stat().st_dev == non_writable_cache_file.stat().st_dev
        assert linked_file.stat().st_ino == non_writable_cache_file.stat().st_ino
