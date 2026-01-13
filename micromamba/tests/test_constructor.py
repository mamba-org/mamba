"""
Tests for micromamba constructor command.

Includes tests for:
- Basic package extraction
- URL-derived metadata handling (issue #4095)
- Consistent field presence (depends/constrains arrays)
"""

import glob
import json
import os
import shutil
import subprocess
import tarfile
import tempfile

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

    @classmethod
    def setup_class(cls):
        """Create a test package with known metadata."""
        cls.temp_dir = tempfile.mkdtemp(prefix="mamba_test_")
        cls.root_prefix = os.path.join(cls.temp_dir, "root")
        cls.pkgs_dir = os.path.join(cls.root_prefix, "pkgs")
        os.makedirs(cls.pkgs_dir, exist_ok=True)

        # Save original env vars
        cls.orig_root_prefix = os.environ.get("MAMBA_ROOT_PREFIX")
        cls.orig_prefix = os.environ.get("CONDA_PREFIX")

        # Set test env vars
        os.environ["MAMBA_ROOT_PREFIX"] = cls.root_prefix
        os.environ["CONDA_PREFIX"] = cls.root_prefix

        # Create a test package with specific metadata in index.json
        cls.pkg_name = "testmeta-1.0-h0_42"
        cls.pkg_filename = cls.pkg_name + ".tar.bz2"
        cls.pkg_path = os.path.join(cls.pkgs_dir, cls.pkg_filename)

        # Create the index.json with specific values that differ from URL-derived stubs
        cls.index_json = {
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

        # Create the package tarball
        cls._create_test_package()

        # Create urls file
        urls_path = os.path.join(cls.pkgs_dir, "urls")
        with open(urls_path, "w") as f:
            # URL with md5 hash fragment (as constructor expects)
            f.write(f"http://test.example.com/channel/linux-64/{cls.pkg_filename}#abc123def456\n")

    @classmethod
    def _create_test_package(cls):
        """Create a minimal conda package tarball."""
        # Create package structure in memory
        pkg_dir = os.path.join(cls.temp_dir, "pkg_build", cls.pkg_name)
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

    @classmethod
    def teardown_class(cls):
        """Clean up test directory and restore env vars."""
        try:
            shutil.rmtree(cls.temp_dir)
        finally:
            # Always restore env vars, even if rmtree fails
            if cls.orig_root_prefix is not None:
                os.environ["MAMBA_ROOT_PREFIX"] = cls.orig_root_prefix
            else:
                os.environ.pop("MAMBA_ROOT_PREFIX", None)
            if cls.orig_prefix is not None:
                os.environ["CONDA_PREFIX"] = cls.orig_prefix
            else:
                os.environ.pop("CONDA_PREFIX", None)

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
