// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <stdexcept>

#include <catch2/catch_all.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/package_fetcher.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"

#include "mambatests.hpp"

namespace
{
    using namespace mamba;

    TEST_CASE("build_download_request")
    {
        auto ctx = Context();
        MultiPackageCache package_caches{ ctx.pkgs_dirs, ctx.validation_params };

        SECTION("From conda-forge")
        {
            static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/pkg-6.4-bld.conda";
            auto pkg_info = specs::PackageInfo::from_url(url).value();

            PackageFetcher pkg_fetcher{ pkg_info, package_caches };
            REQUIRE(pkg_fetcher.name() == pkg_info.name);

            auto req = pkg_fetcher.build_download_request();
            // Should correspond to package name
            REQUIRE(req.name == pkg_info.name);
            // Should correspond to PackageFetcher::channel()
            REQUIRE(req.mirror_name == "");
            // Should correspond to PackageFetcher::url_path()
            REQUIRE(req.url_path == url);
        }

        SECTION("From some mirror")
        {
            static constexpr std::string_view url = "https://repo.prefix.dev/emscripten-forge-dev/emscripten-wasm32/cpp-tabulate-1.5.0-h7223423_2.tar.bz2";
            auto pkg_info = specs::PackageInfo::from_url(url).value();

            PackageFetcher pkg_fetcher{ pkg_info, package_caches };
            REQUIRE(pkg_fetcher.name() == pkg_info.name);

            auto req = pkg_fetcher.build_download_request();
            // Should correspond to package name
            REQUIRE(req.name == pkg_info.name);
            // Should correspond to PackageFetcher::channel()
            REQUIRE(req.mirror_name == "");
            // Should correspond to PackageFetcher::url_path()
            REQUIRE(req.url_path == url);
        }

        SECTION("From local file")
        {
            static constexpr std::string_view url = "file:///home/wolfv/Downloads/xtensor-0.21.4-hc9558a2_0.tar.bz2";
            auto pkg_info = specs::PackageInfo::from_url(url).value();

            PackageFetcher pkg_fetcher{ pkg_info, package_caches };
            REQUIRE(pkg_fetcher.name() == pkg_info.name);

            auto req = pkg_fetcher.build_download_request();
            // Should correspond to package name
            REQUIRE(req.name == pkg_info.name);
            // Should correspond to PackageFetcher::channel()
            REQUIRE(req.mirror_name == "");
            // Should correspond to PackageFetcher::url_path()
            REQUIRE(req.url_path == url);
        }

        SECTION("From oci")
        {
            static constexpr std::string_view url = "oci://ghcr.io/channel-mirrors/conda-forge/linux-64/xtensor-0.25.0-h00ab1b0_0.conda";
            auto pkg_info = specs::PackageInfo::from_url(url).value();

            PackageFetcher pkg_fetcher{ pkg_info, package_caches };
            REQUIRE(pkg_fetcher.name() == pkg_info.name);

            auto req = pkg_fetcher.build_download_request();
            // Should correspond to package name
            REQUIRE(req.name == pkg_info.name);
            // Should correspond to PackageFetcher::channel()
            REQUIRE(req.mirror_name == "oci://ghcr.io/channel-mirrors/conda-forge");
            // Should correspond to PackageFetcher::url_path()
            REQUIRE(req.url_path == "linux-64/xtensor-0.25.0-h00ab1b0_0.conda");
        }
    }

    TEST_CASE("extract_creates_repodata_record_with_dependencies")
    {
        // Test that PackageFetcher.extract() preserves dependencies in repodata_record.json

        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        // Create PackageInfo from URL (exhibits the problematic empty dependencies)
        // Using a noarch package to ensure cross-platform compatibility
        static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/noarch/tzdata-2024a-h0c530f3_0.conda";
        auto pkg_info = specs::PackageInfo::from_url(url).value();

        // Verify precondition: PackageInfo from URL has empty dependencies
        REQUIRE(pkg_info.dependencies.empty());
        REQUIRE(pkg_info.constrains.empty());

        // Extract base filename without extension for reuse
        const std::string pkg_basename = pkg_info.filename.substr(0, pkg_info.filename.size() - 6);

        // Create a minimal but valid conda package structure
        auto pkg_extract_dir = temp_dir.path() / "pkgs" / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // Create index.json with dependencies (what real packages contain)
        nlohmann::json index_json;
        index_json["name"] = pkg_info.name;
        index_json["version"] = pkg_info.version;
        index_json["build"] = pkg_info.build_string;
        index_json["depends"] = nlohmann::json::array({ "python >=3.7" });
        index_json["constrains"] = nlohmann::json::array({ "pytz" });
        index_json["size"] = 123456;

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        // Create minimal required metadata files for a valid conda package
        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        // Create a simple tar.bz2 archive that contains our info directory
        // A .conda file is a zip archive, but let's use .tar.bz2 format for simplicity
        auto tarball_path = temp_dir.path() / "pkgs" / (pkg_basename + ".tar.bz2");

        // Use mamba's create_archive function to create a cross-platform tar.bz2 archive
        create_archive(
            pkg_extract_dir,
            tarball_path,
            compression_algorithm::bzip2,
            /* compression_level= */ 1,
            /* compression_threads= */ 1,
            /* filter= */ nullptr
        );
        REQUIRE(fs::exists(tarball_path));

        // Update pkg_info to use .tar.bz2 format instead of .conda
        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_basename + ".tar.bz2";

        // Clean up the extracted directory so PackageFetcher can extract fresh
        fs::remove_all(pkg_extract_dir);

        // Create PackageFetcher with modified package info
        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };

        // Set up extract options
        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        // Call extract - this is the actual method we're testing
        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        // Verify that repodata_record.json was created with correct dependencies
        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        // Read and parse the created repodata_record.json
        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        REQUIRE(repodata_record.contains("depends"));
        REQUIRE(repodata_record["depends"].is_array());
        REQUIRE(repodata_record["depends"].size() == 1);
        REQUIRE(repodata_record["depends"][0] == "python >=3.7");

        // Also verify constrains is handled correctly
        REQUIRE(repodata_record.contains("constrains"));
        REQUIRE(repodata_record["constrains"].is_array());
        REQUIRE(repodata_record["constrains"].size() == 1);
        REQUIRE(repodata_record["constrains"][0] == "pytz");
    }

    /**
     * Test that URL-derived packages use metadata from index.json
     *
     * PURPOSE: Verify that when extracting URL-derived packages, stub defaults
     * (timestamp=0, license="", build_number=0) are replaced by correct values
     * from the package's index.json.
     *
     * MOTIVATION: Issue #4095 - URL-derived packages incorrectly write stub
     * defaults to repodata_record.json instead of using correct values.
     *
     * SETUP: Create a PackageInfo from URL (has stub values in fields that
     * cannot be parsed from the URL), then extract a tarball with correct
     * metadata in index.json.
     *
     * ASSERTION RATIONALE: The repodata_record.json should contain the CORRECT
     * values from index.json, NOT the stub defaults.
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("PackageFetcher::write_repodata_record uses index.json for URL-derived metadata")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        // Create PackageInfo from URL - this will have stub default values
        // for fields that cannot be parsed from the URL
        static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/test-pkg-1.0-h123456_0.conda";
        auto pkg_info = specs::PackageInfo::from_url(url).value();

        // Verify precondition: PackageInfo from URL has stub defaults
        REQUIRE(pkg_info.timestamp == 0);
        REQUIRE(pkg_info.license == "");
        REQUIRE(pkg_info.build_number == 0);

        const std::string pkg_basename = "test-pkg-1.0-h123456_0";

        // Create a minimal but valid conda package structure
        auto pkg_extract_dir = temp_dir.path() / "pkgs" / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // Create index.json with CORRECT metadata values
        nlohmann::json index_json;
        index_json["name"] = "test-pkg";
        index_json["version"] = "1.0";
        index_json["build"] = "h123456_0";
        index_json["build_number"] = 42;       // Correct value, not 0
        index_json["license"] = "MIT";         // Correct value, not ""
        index_json["timestamp"] = 1234567890;  // Correct value, not 0

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        // Create minimal required metadata files
        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        // Create tar.bz2 archive
        auto tarball_path = temp_dir.path() / "pkgs" / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        // Update pkg_info to use .tar.bz2 format
        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_basename + ".tar.bz2";

        // Clean up and re-extract
        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        // Read repodata_record.json
        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        // Verify repodata_record.json contains correct values from index.json
        // Issue #4095: URL-derived packages should NOT have stub defaults
        CHECK(repodata_record["license"] == "MIT");
        CHECK(repodata_record["timestamp"] == 1234567890);
        CHECK(repodata_record["build_number"] == 42);
    }

    /**
     * Test that channel patches with intentionally empty depends are preserved
     *
     * PURPOSE: Verify that when a channel repodata patch sets depends=[] to fix
     * broken dependencies, this empty array is preserved in repodata_record.json
     * and not replaced by the original index.json depends.
     *
     * MOTIVATION: Issue #4095 - the v2.3.3 partial fix unconditionally erased
     * empty depends[], which silently undid channel patches.
     *
     * SETUP: Create a solver-derived PackageInfo (simulated with empty defaulted_keys
     * except "_initialized") with depends=[] (intentionally empty from channel patch)
     * and timestamp != 0 (non-stub value proves this is solver-derived).
     *
     * ASSERTION RATIONALE: repodata_record["depends"] must be empty array (patch
     * preserved), NOT ["broken-dependency"] from index.json.
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("PackageFetcher::write_repodata_record preserves channel patched empty depends")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        // Create PackageInfo simulating a solver-derived package with patched empty depends
        specs::PackageInfo pkg_info;
        pkg_info.name = "patched-pkg";
        pkg_info.version = "1.0";
        pkg_info.build_string = "h123456_0";
        pkg_info.filename = "patched-pkg-1.0-h123456_0.tar.bz2";
        pkg_info.dependencies = {};                    // Intentionally empty from channel patch
        pkg_info.defaulted_keys = { "_initialized" };  // Only sentinel = solver-derived, trust all
        pkg_info.timestamp = 1234567890;               // Non-zero = proves not URL-derived stub

        const std::string pkg_basename = "patched-pkg-1.0-h123456_0";

        // Create package structure
        auto pkg_extract_dir = temp_dir.path() / "pkgs" / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // Create index.json with broken dependency
        // (This represents the package's original, buggy metadata before patching)
        nlohmann::json index_json;
        index_json["name"] = "patched-pkg";
        index_json["version"] = "1.0";
        index_json["build"] = "h123456_0";
        index_json["depends"] = nlohmann::json::array({ "broken-dependency" });

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        // Create tar.bz2 archive
        auto tarball_path = temp_dir.path() / "pkgs" / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        // Clean up and re-extract
        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        // Read repodata_record.json
        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        // Verify channel patch is preserved (empty depends NOT replaced by index.json)
        // Issue #4095: Solver-derived packages with defaulted_keys={"_initialized"}
        // should trust ALL fields including intentionally empty arrays
        REQUIRE(repodata_record.contains("depends"));
        CHECK(repodata_record["depends"].empty());
    }

    /**
     * Test that channel patches with intentionally empty constrains are preserved
     *
     * PURPOSE: Same as above but for constrains field.
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("PackageFetcher::write_repodata_record preserves channel patched empty constrains")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        // Create PackageInfo simulating a solver-derived package with patched empty constrains
        specs::PackageInfo pkg_info;
        pkg_info.name = "patched-constrains-pkg";
        pkg_info.version = "1.0";
        pkg_info.build_string = "h123456_0";
        pkg_info.filename = "patched-constrains-pkg-1.0-h123456_0.tar.bz2";
        pkg_info.constrains = {};                      // Intentionally empty from channel patch
        pkg_info.defaulted_keys = { "_initialized" };  // Only sentinel = solver-derived, trust all
        pkg_info.timestamp = 1234567890;               // Non-zero = proves not URL-derived stub

        const std::string pkg_basename = "patched-constrains-pkg-1.0-h123456_0";

        // Create package structure
        auto pkg_extract_dir = temp_dir.path() / "pkgs" / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // Create index.json with constrains that were patched away
        nlohmann::json index_json;
        index_json["name"] = "patched-constrains-pkg";
        index_json["version"] = "1.0";
        index_json["build"] = "h123456_0";
        index_json["constrains"] = nlohmann::json::array({ "removed-constraint" });

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        // Create tar.bz2 archive
        auto tarball_path = temp_dir.path() / "pkgs" / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        // Clean up and re-extract
        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        // Read repodata_record.json
        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        // Verify channel patch is preserved (empty constrains NOT replaced by index.json)
        // Issue #4095: Solver-derived packages with defaulted_keys={"_initialized"}
        // should trust ALL fields including intentionally empty arrays
        REQUIRE(repodata_record.contains("constrains"));
        CHECK(repodata_record["constrains"].empty());
    }

    /**
     * Test that extraction fails hard when _initialized sentinel is missing
     *
     * PURPOSE: Verify that write_repodata_record() throws std::logic_error when
     * PackageInfo.defaulted_keys does not contain "_initialized".
     *
     * MOTIVATION: The _initialized sentinel proves the PackageInfo was properly
     * constructed. If missing, it indicates a bug in a code path that creates
     * PackageInfo objects. Failing hard ensures such bugs are caught immediately.
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("PackageFetcher::write_repodata_record fails without _initialized")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        // Create PackageInfo WITHOUT _initialized sentinel - this simulates a bug
        specs::PackageInfo pkg_info;
        pkg_info.name = "missing-init-pkg";
        pkg_info.version = "1.0";
        pkg_info.build_string = "h0_0";
        pkg_info.filename = "missing-init-pkg-1.0-h0_0.tar.bz2";
        // Deliberately NOT setting _initialized in defaulted_keys
        pkg_info.defaulted_keys = { "license", "timestamp" };  // Missing "_initialized"!

        const std::string pkg_basename = "missing-init-pkg-1.0-h0_0";

        // Create minimal package structure
        auto pkg_extract_dir = temp_dir.path() / "pkgs" / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        nlohmann::json index_json;
        index_json["name"] = "missing-init-pkg";
        index_json["version"] = "1.0";
        index_json["build"] = "h0_0";

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        auto tarball_path = temp_dir.path() / "pkgs" / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        // Extraction should throw std::logic_error due to missing _initialized
        REQUIRE_THROWS_AS(pkg_fetcher.extract(options), std::logic_error);
    }

    /**
     * Test that corrupted caches from since v2.1.1 (#3901), partially mitigated in v2.3.3 (#4071)
     * are detected and healed
     *
     * PURPOSE: Verify that EXISTING corrupted repodata_record.json files (from
     * buggy versions since v2.1.1 (#3901), partially mitigated in v2.3.3 (#4071)) are detected via
     * the corruption signature (timestamp=0 AND license=""), invalidated, and automatically
     * re-extracted.
     *
     * MOTIVATION: Issue #4095 - caches corrupted by previous versions persist
     * even after upgrading. The healing mechanism detects and fixes them.
     *
     * HEALING FLOW:
     * 1. has_valid_extracted_dir() reads existing repodata_record.json
     * 2. Detects corruption signature: timestamp=0 AND license=""
     * 3. Returns valid=false → triggers re-extraction
     * 4. write_repodata_record() writes correct values using defaulted_keys
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("PackageFetcher heals existing corrupted cache")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/healing-test-1.0-h123456_0.tar.bz2";
        auto pkg_info = specs::PackageInfo::from_url(url).value();

        const std::string pkg_basename = "healing-test-1.0-h123456_0";

        // Step 1: Create a package with CORRECT index.json
        auto pkg_extract_dir = temp_dir.path() / "pkgs" / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // index.json with CORRECT values that should be used after healing
        nlohmann::json correct_index;
        correct_index["name"] = "healing-test";
        correct_index["version"] = "1.0";
        correct_index["build"] = "h123456_0";
        correct_index["build_number"] = 42;
        correct_index["license"] = "MIT";
        correct_index["timestamp"] = 1234567890;

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << correct_index.dump(2);
        }

        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        // Step 2: Create tar.bz2 archive WITHOUT repodata_record.json (clean package)
        auto tarball_path = temp_dir.path() / "pkgs" / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        // Step 3: Now add CORRUPTED repodata_record.json to cache (simulating since v2.1.1 (#3901),
        // partially mitigated in v2.3.3 (#4071) bug)
        nlohmann::json corrupted_repodata;
        corrupted_repodata["name"] = "healing-test";
        corrupted_repodata["version"] = "1.0";
        corrupted_repodata["build"] = "h123456_0";
        corrupted_repodata["timestamp"] = 0;     // CORRUPTED (stub value)
        corrupted_repodata["license"] = "";      // CORRUPTED (stub value)
        corrupted_repodata["build_number"] = 0;  // CORRUPTED (stub value)
        corrupted_repodata["fn"] = pkg_basename + ".tar.bz2";
        corrupted_repodata["url"] = url;
        corrupted_repodata["depends"] = nlohmann::json::array();
        corrupted_repodata["constrains"] = nlohmann::json::array();

        {
            std::ofstream repodata_file((info_dir / "repodata_record.json").std_path());
            repodata_file << corrupted_repodata.dump(2);
        }

        // Step 4: Update pkg_info to use .tar.bz2 format
        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_basename + ".tar.bz2";

        // Step 5: Create PackageFetcher - it should detect corruption and require re-extraction
        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };

        // Verify healing: corrupted cache should be detected and invalidated.
        // The corruption signature (timestamp=0 AND license="") triggers
        // has_valid_extracted_dir() to return false, causing re-extraction.
        // Issue #4095: This heals caches corrupted by since v2.1.1 (#3901), partially mitigated in
        // v2.3.3 (#4071).
        REQUIRE(pkg_fetcher.needs_extract());

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        // Step 6: Verify that repodata_record.json is now HEALED
        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json healed_repodata;
        repodata_file >> healed_repodata;

        // After healing: correct values from index.json should be present
        CHECK(healed_repodata["license"] == "MIT");
        CHECK(healed_repodata["timestamp"] == 1234567890);
        CHECK(healed_repodata["build_number"] == 42);
    }
}
