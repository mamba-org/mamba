"""
Tests for micromamba constructor command.

Includes tests for:
- Basic package extraction
- URL-derived metadata handling (issue #4095)
- Consistent field presence (depends/constrains arrays)
"""

import glob
import hashlib
import json
import os
import shutil
import subprocess
import tarfile

import pytest

from . import helpers


def constructor(*args, default_channel=True, no_rc=True, no_dry_run=False):
    umamba = helpers.get_umamba()
    cmd = [umamba, "constructor"] + [arg for arg in args if arg]

    try:
        res = subprocess.check_output(cmd)
        if "--json" in args:
            try:
                j = json.loads(res)
                return j
            except json.decoder.JSONDecodeError as e:
                print(f"Error when loading JSON output from {res}")
                raise (e)
        print(f"Error when executing '{' '.join(cmd)}'")
        return res.decode()

    except subprocess.CalledProcessError as e:
        print(f"Error when executing '{' '.join(cmd)}'")
        raise (e)


class TestInstall:
    current_root_prefix = os.environ["MAMBA_ROOT_PREFIX"]
    current_prefix = os.environ["CONDA_PREFIX"]
    cache = os.path.join(current_root_prefix, "pkgs")

    env_name = helpers.random_string()
    root_prefix = os.path.expanduser(os.path.join("~", "tmproot" + helpers.random_string()))
    prefix = os.path.join(root_prefix, "envs", env_name)
    new_cache = os.path.join(root_prefix, "pkgs")

    @classmethod
    def setup_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.prefix

        # speed-up the tests
        os.environ["CONDA_PKGS_DIRS"] = TestInstall.new_cache
        os.makedirs(TestInstall.new_cache, exist_ok=True)
        root_pkgs = glob.glob(os.path.join(TestInstall.current_root_prefix, "pkgs", "x*.tar.bz2"))
        urls = []

        for pkg in root_pkgs:
            shutil.copy(pkg, TestInstall.new_cache)
            urls.append(
                "http://testurl.com/conda-forge/linux-64/" + os.path.basename(pkg) + "#123412341234"
            )

        cls.pkgs = [os.path.basename(pkg) for pkg in root_pkgs]
        with open(os.path.join(TestInstall.new_cache, "urls"), "w") as furls:
            furls.write("\n".join(urls))

    @classmethod
    def teardown_class(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.current_root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.current_prefix
        shutil.rmtree(TestInstall.root_prefix)

    @classmethod
    def teardown_method(cls):
        os.environ["MAMBA_ROOT_PREFIX"] = TestInstall.root_prefix
        os.environ["CONDA_PREFIX"] = TestInstall.prefix

    def test_extract_pkgs(self):
        constructor("--prefix", TestInstall.root_prefix, "--extract-conda-pkgs")

        for pkg in self.pkgs:
            extracted_pkg = os.path.join(TestInstall.root_prefix, "pkgs", pkg.rsplit(".tar.bz2")[0])
            with open(os.path.join(extracted_pkg, "info", "repodata_record.json")) as rr:
                repodata_record = json.load(rr)
            with open(os.path.join(extracted_pkg, "info", "index.json")) as ri:
                index = json.load(ri)
            assert repodata_record["fn"] == pkg
            assert repodata_record["md5"] == "123412341234"
            assert repodata_record["url"] == "http://testurl.com/conda-forge/linux-64/" + pkg
            assert repodata_record["depends"] == index["depends"]


class TestURLDerivedMetadata:
    """
    Tests for URL-derived metadata handling (GitHub issue #4095).

    When packages are installed from URLs (not from solver), the PackageInfo
    is created via from_url() which only has stub values for metadata fields.
    The constructor should merge these with values from index.json.

    Also tests consistent field presence:
    - depends and constrains should always be present as arrays
    - track_features should be omitted when empty
    """

    pkg_name = "testmeta-1.0-h0_42"
    pkg_filename = pkg_name + ".tar.bz2"
    index_json = {
        "name": "testmeta",
        "version": "1.0",
        "build": "h0_42",
        "build_number": 42,
        "license": "MIT",
        "timestamp": 1234567890,
        "depends": ["python >=3.8"],
        "constrains": ["otherpkg >=2.0"],
        # No track_features - should be omitted in output
    }

    @pytest.fixture(autouse=True, scope="class")
    def _setup(self, tmp_path_factory):
        """Set up temp dirs and env vars via fixtures."""
        base = tmp_path_factory.mktemp("mamba_test_url")
        root_prefix = base / "root"
        pkgs_dir = root_prefix / "pkgs"
        pkgs_dir.mkdir(parents=True)

        type(self).root_prefix = str(root_prefix)
        type(self).pkgs_dir = str(pkgs_dir)
        type(self).pkg_path = str(pkgs_dir / self.pkg_filename)

        with pytest.MonkeyPatch.context() as mp:
            mp.setenv("MAMBA_ROOT_PREFIX", self.root_prefix)
            mp.setenv("CONDA_PREFIX", self.root_prefix)

            self._create_test_package(base)

            # Create urls file
            urls_path = os.path.join(self.pkgs_dir, "urls")
            with open(urls_path, "w") as f:
                # URL with md5 hash fragment (as constructor expects)
                f.write(
                    f"http://test.example.com/channel/linux-64/{self.pkg_filename}#abc123def456\n"
                )

            yield

    @classmethod
    def _create_test_package(cls, base):
        """Create a minimal conda package tarball."""
        # Create package structure in memory
        pkg_dir = str(base / "pkg_build" / cls.pkg_name)
        info_dir = os.path.join(pkg_dir, "info")
        os.makedirs(info_dir, exist_ok=True)

        # Write index.json
        with open(os.path.join(info_dir, "index.json"), "w") as f:
            json.dump(cls.index_json, f)

        # Write minimal paths.json
        with open(os.path.join(info_dir, "paths.json"), "w") as f:
            json.dump({"paths": [], "paths_version": 1}, f)

        # Create tarball
        with tarfile.open(cls.pkg_path, "w:bz2") as tar:
            tar.add(info_dir, arcname="info")

    def test_url_derived_metadata_from_index_json(self):
        """
        Test that URL-derived packages get metadata from index.json.

        This is the core test for GitHub issue #4095. When packages are
        installed from URLs, the repodata_record.json should contain
        correct values from index.json, not stub values.

        Specifically tests:
        - license comes from index.json (not empty string)
        - timestamp comes from index.json (not 0)
        - build_number comes from index.json (not 0)
        - depends comes from index.json
        - constrains comes from index.json
        """
        # Run constructor to extract packages
        constructor("--prefix", self.root_prefix, "--extract-conda-pkgs")

        # Read the generated repodata_record.json
        extracted_dir = os.path.join(self.pkgs_dir, self.pkg_name)
        repodata_path = os.path.join(extracted_dir, "info", "repodata_record.json")

        assert os.path.exists(repodata_path), f"repodata_record.json not found at {repodata_path}"

        with open(repodata_path) as f:
            repodata_record = json.load(f)

        # Core issue #4095 checks: these values should come from index.json,
        # not from URL-derived stubs (which would be 0, "", [])
        assert repodata_record.get("license") == "MIT", (
            f"license should be 'MIT' from index.json, got '{repodata_record.get('license')}'"
        )
        assert repodata_record.get("timestamp") == 1234567890, (
            f"timestamp should be 1234567890 from index.json, got {repodata_record.get('timestamp')}"
        )
        assert repodata_record.get("build_number") == 42, (
            f"build_number should be 42 from index.json, got {repodata_record.get('build_number')}"
        )

        # Check depends and constrains match index.json
        assert repodata_record.get("depends") == ["python >=3.8"], (
            f"depends should match index.json, got {repodata_record.get('depends')}"
        )
        assert repodata_record.get("constrains") == ["otherpkg >=2.0"], (
            f"constrains should match index.json, got {repodata_record.get('constrains')}"
        )

    def test_consistent_field_presence(self):
        """
        Test that depends and constrains are always present as arrays.

        Even if they're missing from index.json, they should be present
        as empty arrays in repodata_record.json (matching conda behavior).
        """
        # This test reuses the extraction from the previous test
        extracted_dir = os.path.join(self.pkgs_dir, self.pkg_name)
        repodata_path = os.path.join(extracted_dir, "info", "repodata_record.json")

        if not os.path.exists(repodata_path):
            # Run constructor if not already done
            constructor("--prefix", self.root_prefix, "--extract-conda-pkgs")

        with open(repodata_path) as f:
            repodata_record = json.load(f)

        # depends and constrains should be present as arrays
        assert "depends" in repodata_record, "depends should be present"
        assert isinstance(repodata_record["depends"], list), "depends should be a list"

        assert "constrains" in repodata_record, "constrains should be present"
        assert isinstance(repodata_record["constrains"], list), "constrains should be a list"

    def test_track_features_omitted_when_empty(self):
        """
        Test that track_features is omitted when empty.

        Matches conda behavior to reduce JSON noise.
        """
        extracted_dir = os.path.join(self.pkgs_dir, self.pkg_name)
        repodata_path = os.path.join(extracted_dir, "info", "repodata_record.json")

        if not os.path.exists(repodata_path):
            constructor("--prefix", self.root_prefix, "--extract-conda-pkgs")

        with open(repodata_path) as f:
            repodata_record = json.load(f)

        # track_features was not in index.json, so it should not be in output
        # (or if present, should not be an empty string/array)
        if "track_features" in repodata_record:
            tf = repodata_record["track_features"]
            assert tf, f"track_features should be omitted when empty, got '{tf}'"


class TestChannelPatchPreservation:
    """
    Tests that cached channel repodata values (including channel patches) are preserved.

    This is a regression test for the fix that simplified constructor.cpp to trust
    cached repodata instead of incorrectly erasing fields based on pkg_info.defaulted_keys.

    The old (incorrect) behavior:
    - pkg_info from from_url() has defaulted_keys listing stub fields
    - Those fields were erased from cached repodata (a DIFFERENT object!)
    - Then filled from index.json

    The new (correct) behavior:
    - Trust cached repodata values (they may contain intentional channel patches)
    - Only fill in MISSING keys from index.json

    See GitHub issue #4095.
    """

    pkg_name = "patchtest-1.0-h0_1"
    pkg_filename = pkg_name + ".tar.bz2"
    channel_url = "http://patched.example.com/channel/linux-64/"

    index_json = {
        "name": "patchtest",
        "version": "1.0",
        "build": "h0_1",
        "build_number": 1,
        "license": "BSD-3-Clause",
        "timestamp": 1000000000,
        "depends": ["libfoo >=1.0"],
        "constrains": [],
    }

    # Values in cached channel repodata (simulating channel patches)
    # These DIFFER from index.json - the channel maintainer patched them
    patched_repodata = {
        "name": "patchtest",
        "version": "1.0",
        "build": "h0_1",
        "build_number": 1,
        # Channel patch: changed license
        "license": "MIT",
        # Channel patch: different timestamp
        "timestamp": 2000000000,
        # Channel patch: added dependency constraint
        "depends": ["libfoo >=1.0", "libbar <2.0"],
        # Channel patch: added constrains
        "constrains": ["conflicting-pkg"],
    }

    @pytest.fixture(autouse=True, scope="class")
    def _setup(self, tmp_path_factory):
        """Set up temp dirs and env vars via fixtures."""
        base = tmp_path_factory.mktemp("mamba_test_channel_patch")
        root_prefix = base / "root"
        pkgs_dir = root_prefix / "pkgs"
        cache_dir = pkgs_dir / "cache"
        cache_dir.mkdir(parents=True)

        type(self).root_prefix = str(root_prefix)
        type(self).pkgs_dir = str(pkgs_dir)
        type(self).pkg_path = str(pkgs_dir / self.pkg_filename)

        with pytest.MonkeyPatch.context() as mp:
            mp.setenv("MAMBA_ROOT_PREFIX", self.root_prefix)
            mp.setenv("CONDA_PREFIX", self.root_prefix)

            self._create_test_package(base)
            self._create_cached_repodata(str(cache_dir))

            # Create urls file
            urls_path = os.path.join(self.pkgs_dir, "urls")
            with open(urls_path, "w") as f:
                f.write(f"{self.channel_url}{self.pkg_filename}#deadbeef12345678\n")

            yield

    @classmethod
    def _create_test_package(cls, base):
        """Create a minimal conda package tarball with index.json."""
        pkg_dir = str(base / "pkg_build" / cls.pkg_name)
        info_dir = os.path.join(pkg_dir, "info")
        os.makedirs(info_dir, exist_ok=True)

        # Write index.json with ORIGINAL (non-patched) values
        with open(os.path.join(info_dir, "index.json"), "w") as f:
            json.dump(cls.index_json, f)

        # Write minimal paths.json
        with open(os.path.join(info_dir, "paths.json"), "w") as f:
            json.dump({"paths": [], "paths_version": 1}, f)

        # Create tarball
        with tarfile.open(cls.pkg_path, "w:bz2") as tar:
            tar.add(info_dir, arcname="info")

    @classmethod
    def _create_cached_repodata(cls, cache_dir):
        """Create cached channel repodata with patched values."""
        repodata = {
            "packages": {
                cls.pkg_filename: cls.patched_repodata,
            },
            "packages.conda": {},
        }

        # Compute MD5 of channel URL to match C++ cache_name_from_url()
        url_hash = hashlib.md5(cls.channel_url.encode()).hexdigest()[:8]
        cache_filename = f"{url_hash}.json"

        cache_path = os.path.join(cache_dir, cache_filename)
        with open(cache_path, "w") as f:
            json.dump(repodata, f)

    def test_channel_patches_preserved(self):
        """
        Test that channel-patched values in cached repodata are preserved.

        This is a RED-GREEN test:
        - RED (old code): Would erase 'license', 'timestamp', 'depends', 'constrains'
          from cached repodata based on pkg_info.defaulted_keys, then fill from
          index.json, losing the channel patches.
        - GREEN (new code): Trusts cached repodata, preserving channel patches.
          Only fills in MISSING keys from index.json.

        The test verifies that the patched values (which differ from index.json)
        are preserved in the final repodata_record.json.
        """
        # Run constructor to extract packages
        constructor("--prefix", self.root_prefix, "--extract-conda-pkgs")

        # Read the generated repodata_record.json
        extracted_dir = os.path.join(self.pkgs_dir, self.pkg_name)
        repodata_path = os.path.join(extracted_dir, "info", "repodata_record.json")

        assert os.path.exists(repodata_path), f"repodata_record.json not found at {repodata_path}"

        with open(repodata_path) as f:
            repodata_record = json.load(f)

        # These assertions would FAIL with the old code (which overwrote with index.json)
        # and PASS with the new code (which preserves cached repodata)

        # Channel-patched license should be preserved
        assert repodata_record.get("license") == "MIT", (
            f"Channel-patched license 'MIT' should be preserved, "
            f"got '{repodata_record.get('license')}' (index.json has 'BSD-3-Clause')"
        )

        # Channel-patched timestamp should be preserved
        assert repodata_record.get("timestamp") == 2000000000, (
            f"Channel-patched timestamp 2000000000 should be preserved, "
            f"got {repodata_record.get('timestamp')} (index.json has 1000000000)"
        )

        # Channel-patched depends should be preserved (has extra dependency)
        expected_depends = ["libfoo >=1.0", "libbar <2.0"]
        assert repodata_record.get("depends") == expected_depends, (
            f"Channel-patched depends {expected_depends} should be preserved, "
            f"got {repodata_record.get('depends')} (index.json has ['libfoo >=1.0'])"
        )

        # Channel-patched constrains should be preserved
        expected_constrains = ["conflicting-pkg"]
        assert repodata_record.get("constrains") == expected_constrains, (
            f"Channel-patched constrains {expected_constrains} should be preserved, "
            f"got {repodata_record.get('constrains')} (index.json has [])"
        )
