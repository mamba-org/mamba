import os
import sys
import platform
from pathlib import Path

import pytest

# Need to import everything to get fixtures
from .helpers import *  # noqa: F403
from . import helpers

package_to_test = "xtensor"
file_in_package_to_test = "xtensor.hpp"


class TestLinking:
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]

    env_name = helpers.random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + helpers.random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestLinking.root_prefix

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestLinking.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestLinking.current_prefix

        if Path(TestLinking.root_prefix).exists():
            helpers.rmtree(TestLinking.root_prefix)

    @classmethod
    def teardown_method(cls):
        if Path(TestLinking.prefix).exists():
            helpers.rmtree(TestLinking.prefix)

    def test_link(self, existing_cache, test_pkg):
        helpers.create(package_to_test, "-n", TestLinking.env_name, "--json", no_dry_run=True)

        install_env_dir = helpers.get_env(TestLinking.env_name)
        pkg_checker = helpers.PackageChecker(package_to_test, install_env_dir)
        linked_file_path = pkg_checker.find_installed(file_in_package_to_test)
        assert linked_file_path
        assert linked_file_path.exists()
        assert not linked_file_path.is_symlink()

        linked_file_rel_path = linked_file_path.relative_to(install_env_dir)
        # test_pkg is now a Path object (relative or absolute) pointing to the package directory
        if test_pkg.is_absolute():
            cache_file = test_pkg / linked_file_rel_path
        else:
            cache_file = existing_cache / test_pkg / linked_file_rel_path
        assert cache_file.stat().st_dev == linked_file_path.stat().st_dev
        assert cache_file.stat().st_ino == linked_file_path.stat().st_ino

    def test_copy(self, existing_cache, test_pkg):
        helpers.create(
            package_to_test,
            "-n",
            TestLinking.env_name,
            "--json",
            "--always-copy",
            no_dry_run=True,
        )
        install_env_dir = helpers.get_env(TestLinking.env_name)
        pkg_checker = helpers.PackageChecker(package_to_test, install_env_dir)
        linked_file_path = pkg_checker.find_installed(file_in_package_to_test)
        assert linked_file_path
        assert linked_file_path.exists()
        assert not linked_file_path.is_symlink()

        linked_file_rel_path = linked_file_path.relative_to(install_env_dir)
        # test_pkg is now a Path object (relative or absolute) pointing to the package directory
        if test_pkg.is_absolute():
            cache_file = test_pkg / linked_file_rel_path
        else:
            cache_file = existing_cache / test_pkg / linked_file_rel_path
        assert cache_file.stat().st_dev == linked_file_path.stat().st_dev
        assert cache_file.stat().st_ino != linked_file_path.stat().st_ino

    @pytest.mark.skipif(
        platform.system() == "Windows",
        reason="Softlinking needs admin privileges on win",
    )
    def test_always_softlink(self, existing_cache, test_pkg):
        helpers.create(
            package_to_test,
            "-n",
            TestLinking.env_name,
            "--json",
            "--always-softlink",
            no_dry_run=True,
        )
        install_env_dir = helpers.get_env(TestLinking.env_name)
        pkg_checker = helpers.PackageChecker(package_to_test, install_env_dir)
        linked_file_path = pkg_checker.find_installed(file_in_package_to_test)
        assert linked_file_path
        assert linked_file_path.exists()
        assert linked_file_path.is_symlink()

        linked_file_rel_path = linked_file_path.relative_to(install_env_dir)
        # test_pkg is now a Path object (relative or absolute) pointing to the package directory
        if test_pkg.is_absolute():
            cache_file = test_pkg / linked_file_rel_path
        else:
            cache_file = existing_cache / test_pkg / linked_file_rel_path

        assert cache_file.stat().st_dev == linked_file_path.stat().st_dev
        assert cache_file.stat().st_ino == linked_file_path.stat().st_ino
        assert os.readlink(linked_file_path) == str(cache_file)

    @pytest.mark.parametrize("allow_softlinks", [True, False])
    @pytest.mark.parametrize("always_copy", [True, False])
    def test_cross_device(self, allow_softlinks, always_copy, existing_cache, test_pkg):
        if platform.system() != "Linux":
            pytest.skip("o/s is not linux")

        create_args = [package_to_test, "-n", TestLinking.env_name, "--json"]
        if allow_softlinks:
            create_args.append("--allow-softlinks")
        if always_copy:
            create_args.append("--always-copy")
        helpers.create(*create_args, no_dry_run=True)

        same_device = existing_cache.stat().st_dev == Path(TestLinking.prefix).stat().st_dev
        is_softlink = not same_device and allow_softlinks and not always_copy
        is_hardlink = same_device and not always_copy

        install_env_dir = helpers.get_env(TestLinking.env_name)
        pkg_checker = helpers.PackageChecker(package_to_test, install_env_dir)
        linked_file_path = pkg_checker.find_installed(file_in_package_to_test)
        assert linked_file_path
        assert linked_file_path.exists()

        linked_file_rel_path = linked_file_path.relative_to(install_env_dir)
        # test_pkg is now a Path object (relative or absolute) pointing to the package directory
        if test_pkg.is_absolute():
            cache_file = test_pkg / linked_file_rel_path
        else:
            cache_file = existing_cache / test_pkg / linked_file_rel_path
        assert cache_file.stat().st_dev == linked_file_path.stat().st_dev
        assert (cache_file.stat().st_ino == linked_file_path.stat().st_ino) == is_hardlink
        assert linked_file_path.is_symlink() == is_softlink

    def test_unlink_missing_file(self):
        helpers.create(package_to_test, "-n", TestLinking.env_name, "--json", no_dry_run=True)

        pkg_checker = helpers.PackageChecker(package_to_test, helpers.get_env(TestLinking.env_name))
        linked_file_path = pkg_checker.find_installed(file_in_package_to_test)
        assert linked_file_path
        assert linked_file_path.exists()
        assert not linked_file_path.is_symlink()

        os.remove(linked_file_path)
        helpers.remove(package_to_test, "-n", TestLinking.env_name)

    @pytest.mark.skipif(
        sys.platform == "darwin" and platform.machine() == "arm64",
        reason="Python 3.7 not available",
    )
    def test_link_missing_scripts_dir(self):  # issue 2808
        helpers.create("python=3.7", "pypy", "-n", TestLinking.env_name, "--json", no_dry_run=True)
