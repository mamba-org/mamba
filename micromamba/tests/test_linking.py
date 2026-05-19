import json
import os
import platform
import sys
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

    # httpx unlink / conda-metadata interoperability tests
    _HTTPX_UNLINK_OLD_VER = "0.27.0"
    _HTTPX_UNLINK_NEW_VER = "0.28.1"

    @staticmethod
    def _httpx_dist_info_resolved(env_prefix: Path, httpx_version: str) -> set[Path]:
        """Paths to httpx-*.dist-info under lib/Lib, deduplicated by real path.

        Conda Python may expose ``lib/python3.1`` as a symlink to ``lib/python3.10``;
        ``python*/site-packages/`` then matches the same directory twice.
        On Windows, site-packages live under ``Lib/site-packages/``.
        """
        marker = f"httpx-{httpx_version}.dist-info"
        patterns = (
            f"python*/site-packages/{marker}",
            f"site-packages/{marker}",
        )
        found: set[Path] = set()
        for lib_dir in ("lib", "Lib"):
            base = env_prefix / lib_dir
            if not base.is_dir():
                continue
            for pattern in patterns:
                found.update(p.resolve() for p in base.glob(pattern))
        return found

    @staticmethod
    def _resolved_paths_from_conda_meta_record(env_prefix: Path, record: dict) -> set[Path]:
        """Absolute resolved paths declared by a conda-meta package record."""
        rels: list[str] = []
        if "paths_data" in record:
            rels = [entry["_path"] for entry in record["paths_data"]["paths"]]
        elif "files" in record:
            rels = list(record["files"])
        return {(env_prefix / rel).resolve() for rel in rels}

    @staticmethod
    def _paths_under_httpx_dist_info(paths: set[Path], httpx_version: str) -> set[Path]:
        marker = f"httpx-{httpx_version}.dist-info"
        return {p for p in paths if marker in p.parts}

    @staticmethod
    def _assert_paths_removed(paths: set[Path], *, label: str) -> None:
        stale = sorted(p for p in paths if p.exists())
        assert not stale, f"{label} should be removed but still exist: {stale}"

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

    def test_unlink_legacy_files_metadata(self):
        old_ver = self._HTTPX_UNLINK_OLD_VER
        new_ver = self._HTTPX_UNLINK_NEW_VER

        helpers.create(
            "python=3.10",
            f"httpx={old_ver}",
            "-n",
            TestLinking.env_name,
            "--json",
            no_dry_run=True,
        )

        env_prefix = Path(helpers.get_env(TestLinking.env_name))
        old_meta = next((env_prefix / "conda-meta").glob(f"httpx-{old_ver}-*.json"))
        with old_meta.open() as f:
            old_record = json.load(f)

        prev_dist_info_paths = self._paths_under_httpx_dist_info(
            self._resolved_paths_from_conda_meta_record(env_prefix, old_record),
            old_ver,
        )
        assert prev_dist_info_paths, "expected previous httpx dist-info files before upgrade"

        # Simulate conda metadata that only stores the legacy `files` list.
        old_record["files"] = [p["_path"] for p in old_record["paths_data"]["paths"]]
        old_record.pop("paths_data", None)
        with old_meta.open("w") as f:
            json.dump(old_record, f)

        helpers.install(
            f"httpx={new_ver}",
            "-n",
            TestLinking.env_name,
            "--json",
            no_dry_run=True,
        )

        self._assert_paths_removed(
            prev_dist_info_paths,
            label=f"httpx {old_ver} dist-info",
        )
        assert len(self._httpx_dist_info_resolved(env_prefix, old_ver)) == 0
        assert len(self._httpx_dist_info_resolved(env_prefix, new_ver)) == 1

    def test_unlink_noarch_short_paths_metadata(self):
        old_ver = self._HTTPX_UNLINK_OLD_VER
        new_ver = self._HTTPX_UNLINK_NEW_VER

        helpers.create(
            "python=3.10",
            f"httpx={old_ver}",
            "-n",
            TestLinking.env_name,
            "--json",
            no_dry_run=True,
        )

        env_prefix = Path(helpers.get_env(TestLinking.env_name))
        old_meta = next((env_prefix / "conda-meta").glob(f"httpx-{old_ver}-*.json"))
        with old_meta.open() as f:
            old_record = json.load(f)

        prev_dist_info_paths = self._paths_under_httpx_dist_info(
            self._resolved_paths_from_conda_meta_record(env_prefix, old_record),
            old_ver,
        )
        assert prev_dist_info_paths, "expected previous httpx dist-info files before upgrade"

        # Simulate conda-generated noarch metadata where `paths_data.paths[*]._path`
        # uses short paths like `site-packages/...` instead of full
        # `lib/pythonX.Y/site-packages/...` or `Lib/site-packages/...` entries.
        for path in old_record["paths_data"]["paths"]:
            current_path = path.get("_path", "")
            if current_path.startswith("lib/python") and "/site-packages/" in current_path:
                path["_path"] = current_path.split("/site-packages/", 1)[1]
                path["_path"] = f"site-packages/{path['_path']}"
            elif current_path.startswith("Lib/site-packages/"):
                path["_path"] = current_path.removeprefix("Lib/site-packages/")
                path["_path"] = f"site-packages/{path['_path']}"

        with old_meta.open("w") as f:
            json.dump(old_record, f)

        helpers.install(
            f"httpx={new_ver}",
            "-n",
            TestLinking.env_name,
            "--json",
            no_dry_run=True,
        )

        self._assert_paths_removed(
            prev_dist_info_paths,
            label=f"httpx {old_ver} dist-info",
        )
        assert len(self._httpx_dist_info_resolved(env_prefix, old_ver)) == 0
        assert len(self._httpx_dist_info_resolved(env_prefix, new_ver)) == 1

    @pytest.mark.skipif(
        sys.platform == "darwin" and platform.machine() == "arm64",
        reason="Python 3.7 not available",
    )
    def test_link_missing_scripts_dir(self):  # issue 2808
        helpers.create("python=3.7", "pypy", "-n", TestLinking.env_name, "--json", no_dry_run=True)
