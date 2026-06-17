// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <stdexcept>

#include <catch2/catch_all.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/package_fetcher.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/url_manip.hpp"

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

        // Use the same cache layout as PackageFetcher
        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        // Create a minimal but valid conda package structure
        auto pkg_extract_dir = cache_subdir / pkg_basename;
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
        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");

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

    TEST_CASE("package_cache_folder_relative_path")
    {
        using namespace mamba;

        SECTION("HTTPS URL with noarch platform")
        {
            specs::PackageInfo pkg;
            pkg.name = "python_abi";
            pkg.package_url = "https://prefix.dev/conda-forge/noarch/python_abi-3.14-8_cp314.conda";
            pkg.filename = "python_abi-3.14-8_cp314.conda";
            pkg.platform = "noarch";
            pkg.channel = "conda-forge";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("https/prefix.dev/conda-forge/noarch").string());
        }

        SECTION("HTTPS URL with linux-64 platform")
        {
            specs::PackageInfo pkg;
            pkg.name = "numpy";
            pkg.package_url = "https://conda.anaconda.org/conda-forge/linux-64/numpy-1.26.0-py311h1234567_0.conda";
            pkg.filename = "numpy-1.26.0-py311h1234567_0.conda";
            pkg.platform = "linux-64";
            pkg.channel = "conda-forge";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(
                result.string() == fs::u8path("https/conda.anaconda.org/conda-forge/linux-64").string()
            );
        }

        SECTION("HTTPS URL with custom domain and deep path")
        {
            specs::PackageInfo pkg;
            pkg.name = "test-package";
            pkg.package_url = "https://repo.example.com/channels/my-channel/linux-64/test-package-1.0-0.tar.bz2";
            pkg.filename = "test-package-1.0-0.tar.bz2";
            pkg.platform = "linux-64";
            pkg.channel = "my-channel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(
                result.string()
                == fs::u8path("https/repo.example.com/channels/my-channel/linux-64").string()
            );
        }

        SECTION("HTTP URL (non-secure)")
        {
            specs::PackageInfo pkg;
            pkg.name = "test-pkg";
            pkg.package_url = "http://localhost:8000/mychannel/noarch/test-pkg-0.1-0.tar.bz2";
            pkg.filename = "test-pkg-0.1-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "mychannel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("http/localhost_8000/mychannel/noarch").string());
        }

        SECTION("OCI registry URL")
        {
            specs::PackageInfo pkg;
            pkg.name = "xtensor";
            pkg.package_url = "oci://ghcr.io/channel-mirrors/conda-forge/linux-64/xtensor-0.25.0-h00ab1b0_0.conda";
            pkg.filename = "xtensor-0.25.0-h00ab1b0_0.conda";
            pkg.platform = "linux-64";
            pkg.channel = "conda-forge";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(
                result.string()
                == fs::u8path("oci/ghcr.io/channel-mirrors/conda-forge/linux-64").string()
            );
        }

        SECTION("OCI registry with different host")
        {
            specs::PackageInfo pkg;
            pkg.name = "package";
            pkg.package_url = "oci://registry.example.com:5000/myorg/mychannel/noarch/package-1.0-0.conda";
            pkg.filename = "package-1.0-0.conda";
            pkg.platform = "noarch";
            pkg.channel = "mychannel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(
                result.string()
                == fs::u8path("oci/registry.example.com_5000/myorg/mychannel/noarch").string()
            );
        }

        SECTION("File URL")
        {
            specs::PackageInfo pkg;
            pkg.name = "local-pkg";
            pkg.package_url = "file:///home/user/packages/linux-64/local-pkg-1.0-0.tar.bz2";
            pkg.filename = "local-pkg-1.0-0.tar.bz2";
            pkg.platform = "linux-64";
            pkg.channel = "local";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("file//home/user/packages/linux-64").string());
        }

        SECTION("File URL with Windows path")
        {
            specs::PackageInfo pkg;
            pkg.name = "win-pkg";
            pkg.package_url = "file:///C:/Users/name/packages/win-64/win-pkg-1.0-0.tar.bz2";
            pkg.filename = "win-pkg-1.0-0.tar.bz2";
            pkg.platform = "win-64";
            pkg.channel = "local";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("file//C_/Users/name/packages/win-64").string());
        }

        SECTION("HTTPS URL with authentication (credentials in URL)")
        {
            specs::PackageInfo pkg;
            pkg.name = "auth-pkg";
            pkg.package_url = "https://user:pass@repo.example.com/channel/noarch/auth-pkg-1.0-0.tar.bz2";
            pkg.filename = "auth-pkg-1.0-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "channel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("https/repo.example.com/channel/noarch").string());
        }

        SECTION("URL with port number")
        {
            specs::PackageInfo pkg;
            pkg.name = "port-pkg";
            pkg.package_url = "https://repo.example.com:8443/channel/linux-64/port-pkg-1.0-0.tar.bz2";
            pkg.filename = "port-pkg-1.0-0.tar.bz2";
            pkg.platform = "linux-64";
            pkg.channel = "channel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(
                result.string() == fs::u8path("https/repo.example.com_8443/channel/linux-64").string()
            );
        }

        SECTION("URL with query parameters")
        {
            specs::PackageInfo pkg;
            pkg.name = "query-pkg";
            pkg.package_url = "https://repo.example.com/channel/noarch/query-pkg-1.0-0.tar.bz2?token=abc123";
            pkg.filename = "query-pkg-1.0-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "channel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("https/repo.example.com/channel/noarch").string());
        }

        SECTION("URL with fragment")
        {
            specs::PackageInfo pkg;
            pkg.name = "frag-pkg";
            pkg.package_url = "https://repo.example.com/channel/noarch/frag-pkg-1.0-0.tar.bz2#section";
            pkg.filename = "frag-pkg-1.0-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "channel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("https/repo.example.com/channel/noarch").string());
        }

        SECTION("Fallback to channel/platform when package_url is empty")
        {
            specs::PackageInfo pkg;
            pkg.name = "fallback-pkg";
            pkg.package_url = "";
            pkg.filename = "fallback-pkg-1.0-0.tar.bz2";
            pkg.platform = "linux-64";
            pkg.channel = "conda-forge";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("conda-forge/linux-64").string());
        }

        SECTION("Fallback with channel containing platform")
        {
            specs::PackageInfo pkg;
            pkg.name = "fallback-pkg2";
            pkg.package_url = "";
            pkg.filename = "fallback-pkg2-1.0-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "https://repo.example.com/channel/noarch";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("https/repo.example.com/channel/noarch").string());
        }

        SECTION("URL with trailing slash")
        {
            specs::PackageInfo pkg;
            pkg.name = "trailing-pkg";
            pkg.package_url = "https://repo.example.com/channel/noarch/";
            pkg.filename = "trailing-pkg-1.0-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "channel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("https/repo.example.com/channel/noarch").string());
        }

        SECTION("Complex OCI URL with nested paths")
        {
            specs::PackageInfo pkg;
            pkg.name = "complex-oci";
            pkg.package_url = "oci://registry.example.com/org/team/project/channel/linux-64/complex-oci-2.0-1.conda";
            pkg.filename = "complex-oci-2.0-1.conda";
            pkg.platform = "linux-64";
            pkg.channel = "channel";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(
                result.string()
                == fs::u8path("oci/registry.example.com/org/team/project/channel/linux-64").string()
            );
        }

        SECTION("URL with special characters in path")
        {
            specs::PackageInfo pkg;
            pkg.name = "special-pkg";
            pkg.package_url = "https://repo.example.com/channel-name/sub-channel/noarch/special-pkg-1.0-0.tar.bz2";
            pkg.filename = "special-pkg-1.0-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "channel-name";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(
                result.string()
                == fs::u8path("https/repo.example.com/channel-name/sub-channel/noarch").string()
            );
        }

        SECTION("URL where filename doesn't match package_url ending")
        {
            specs::PackageInfo pkg;
            pkg.name = "mismatch-pkg";
            pkg.package_url = "https://repo.example.com/channel/noarch/some-other-file.tar.bz2";
            pkg.filename = "mismatch-pkg-1.0-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "channel";

            auto result = package_cache_folder_relative_path(pkg);
            // Should extract directory by finding last '/'
            REQUIRE(result.string() == fs::u8path("https/repo.example.com/channel/noarch").string());
        }

        SECTION("Empty package_url and empty channel fallback")
        {
            specs::PackageInfo pkg;
            pkg.name = "empty-pkg";
            pkg.package_url = "";
            pkg.filename = "empty-pkg-1.0-0.tar.bz2";
            pkg.platform = "noarch";
            pkg.channel = "";

            auto result = package_cache_folder_relative_path(pkg);
            REQUIRE(result.string() == fs::u8path("no_channel/noarch").string());
        }
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

        // Use the same cache layout as PackageFetcher
        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        // Create a minimal but valid conda package structure
        auto pkg_extract_dir = cache_subdir / pkg_basename;
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
        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
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
     * Comprehensive test for URL-derived repodata_record.json correctness.
     *
     * Uses a file:// URL (which lacks a platform path segment) to exercise
     * the worst case: subdir is empty and must be backfilled from index.json.
     * Verifies every metadata field against expected values.
     *
     * This is the C++ equivalent of reproduce_4095.py.
     */
    TEST_CASE("PackageFetcher::write_repodata_record comprehensive URL-derived fields")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        const std::string pkg_basename = "test-pkg-1.0-h123456_0";
        const std::string pkg_filename = pkg_basename + ".tar.bz2";

        // file:// URL has no platform segment → subdir must be backfilled
        auto pkg_info = specs::PackageInfo::from_url(
                            util::path_to_url((temp_dir.path() / "src" / pkg_filename).string())
        )
                            .value();

        REQUIRE(pkg_info.platform.empty());

        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);
        auto pkg_extract_dir = cache_subdir / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        nlohmann::json index_json;
        index_json["name"] = "test-pkg";
        index_json["version"] = "1.0";
        index_json["build"] = "h123456_0";
        index_json["build_number"] = 42;
        index_json["license"] = "BSD-3-Clause";
        index_json["timestamp"] = 9876543210;
        index_json["subdir"] = "linux-64";
        index_json["noarch"] = "python";
        index_json["depends"] = nlohmann::json::array({ "python >=3.8", "yaml" });
        index_json["constrains"] = nlohmann::json::array({ "otherpkg >=2.0" });
        index_json["track_features"] = "some_feature";

        {
            std::ofstream f((info_dir / "index.json").std_path());
            f << index_json.dump(2);
        }
        {
            std::ofstream f((info_dir / "paths.json").std_path());
            f << R"({"paths": [], "paths_version": 1})";
        }

        auto tarball_path = cache_subdir / pkg_filename;
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_filename;

        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };
        REQUIRE(pkg_fetcher.extract(
            { .sparse = false, .subproc_mode = extract_subproc_mode::mamba_package }
        ));

        auto rr_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(rr_path));

        std::ifstream rr_file(rr_path.std_path());
        nlohmann::json rr;
        rr_file >> rr;

        // Fields from URL (should be preserved as-is)
        CHECK(rr["name"] == "test-pkg");
        CHECK(rr["version"] == "1.0");
        CHECK(rr["build"] == "h123456_0");
        CHECK(rr["build_string"] == "h123456_0");
        CHECK(rr["fn"] == pkg_filename);
        CHECK_FALSE(rr["url"].get<std::string>().empty());

        // Fields backfilled from index.json (the core issue #4095 fix)
        CHECK(rr["license"] == "BSD-3-Clause");
        CHECK(rr["build_number"] == 42);
        CHECK(rr["timestamp"] == 9876543210);
        CHECK(rr["subdir"] == "linux-64");
        CHECK(rr["noarch"] == "python");
        CHECK(rr["depends"] == nlohmann::json::array({ "python >=3.8", "yaml" }));
        CHECK(rr["constrains"] == nlohmann::json::array({ "otherpkg >=2.0" }));
        CHECK(rr["track_features"] == "some_feature");

        // Checksums computed from tarball
        CHECK(rr.contains("md5"));
        CHECK(rr["md5"].is_string());
        CHECK_FALSE(rr["md5"].get<std::string>().empty());
        CHECK(rr.contains("sha256"));
        CHECK(rr["sha256"].is_string());
        CHECK_FALSE(rr["sha256"].get<std::string>().empty());

        // Size from tarball
        CHECK(rr["size"] > 0);
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
        pkg_info.dependencies = {};  // Intentionally empty from channel patch
        pkg_info.defaulted_keys = { std::string(specs::defaulted_key::initialized) };
        pkg_info.timestamp = 1234567890;  // Non-zero = proves not URL-derived stub

        const std::string pkg_basename = "patched-pkg-1.0-h123456_0";

        // Use the same cache layout as PackageFetcher
        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        // Create package structure
        auto pkg_extract_dir = cache_subdir / pkg_basename;
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
        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
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
        pkg_info.constrains = {};  // Intentionally empty from channel patch
        pkg_info.defaulted_keys = { std::string(specs::defaulted_key::initialized) };
        pkg_info.timestamp = 1234567890;  // Non-zero = proves not URL-derived stub

        const std::string pkg_basename = "patched-constrains-pkg-1.0-h123456_0";

        // Use the same cache layout as PackageFetcher
        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        // Create package structure
        auto pkg_extract_dir = cache_subdir / pkg_basename;
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
        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
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
        pkg_info.defaulted_keys = { std::string(specs::defaulted_key::license),
                                    std::string(specs::defaulted_key::timestamp) };

        const std::string pkg_basename = "missing-init-pkg-1.0-h0_0";

        // Use the same cache layout as PackageFetcher
        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        // Create minimal package structure
        auto pkg_extract_dir = cache_subdir / pkg_basename;
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

        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
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
     * Test that depends and constrains are always present as arrays
     *
     * PURPOSE: Verify that repodata_record.json always includes "depends" and
     * "constrains" fields as arrays, even when they're missing from index.json.
     *
     * MOTIVATION: Matches conda behavior where these fields are always present.
     * Some packages (like nlohmann_json-abi) don't have depends in index.json.
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("PackageFetcher::write_repodata_record ensures depends/constrains present")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/nodeps-1.0-h0_0.conda";
        auto pkg_info = specs::PackageInfo::from_url(url).value();

        const std::string pkg_basename = "nodeps-1.0-h0_0";

        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        auto pkg_extract_dir = cache_subdir / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // Create index.json WITHOUT depends or constrains (like nlohmann_json-abi)
        nlohmann::json index_json;
        index_json["name"] = "nodeps";
        index_json["version"] = "1.0";
        index_json["build"] = "h0_0";
        // NO depends or constrains keys

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_basename + ".tar.bz2";

        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        // depends and constrains should always be present as empty arrays
        REQUIRE(repodata_record.contains("depends"));
        CHECK(repodata_record["depends"].is_array());
        CHECK(repodata_record["depends"].empty());

        REQUIRE(repodata_record.contains("constrains"));
        CHECK(repodata_record["constrains"].is_array());
        CHECK(repodata_record["constrains"].empty());
    }

    /**
     * Test that track_features is omitted when empty
     *
     * PURPOSE: Verify that track_features is only included in repodata_record.json
     * when it has non-empty value. Matches conda behavior.
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("PackageFetcher::write_repodata_record omits empty track_features")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/notf-1.0-h0_0.conda";
        auto pkg_info = specs::PackageInfo::from_url(url).value();

        const std::string pkg_basename = "notf-1.0-h0_0";

        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        auto pkg_extract_dir = cache_subdir / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // Create index.json without track_features
        nlohmann::json index_json;
        index_json["name"] = "notf";
        index_json["version"] = "1.0";
        index_json["build"] = "h0_0";
        // NO track_features key

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_basename + ".tar.bz2";

        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        // track_features should be omitted when empty
        CHECK_FALSE(repodata_record.contains("track_features"));
    }

    /**
     * Test that both checksums are always present
     *
     * PURPOSE: Verify that both md5 and sha256 checksums are always present in
     * repodata_record.json, computed from tarball if not available.
     *
     * Related: https://github.com/mamba-org/mamba/issues/4095
     */
    TEST_CASE("PackageFetcher::write_repodata_record ensures both checksums")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/nosum-1.0-h0_0.conda";
        auto pkg_info = specs::PackageInfo::from_url(url).value();

        const std::string pkg_basename = "nosum-1.0-h0_0";

        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        auto pkg_extract_dir = cache_subdir / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // Create index.json without checksums (which is normal)
        nlohmann::json index_json;
        index_json["name"] = "nosum";
        index_json["version"] = "1.0";
        index_json["build"] = "h0_0";
        // NO md5 or sha256

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_basename + ".tar.bz2";
        // Note: pkg_info has empty md5 and sha256 (not from URL hash)

        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        // Both checksums should be present (computed from tarball)
        REQUIRE(repodata_record.contains("md5"));
        CHECK_FALSE(repodata_record["md5"].get<std::string>().empty());

        REQUIRE(repodata_record.contains("sha256"));
        CHECK_FALSE(repodata_record["sha256"].get<std::string>().empty());
    }

    /**
     * Test that noarch and python_site_packages_path are backfilled from index.json
     *
     * PURPOSE: Verify fields that to_json() conditionally writes are correctly
     * filled from index.json via insert().
     *
     * These fields are NOT in defaulted_keys because to_json() omits them when
     * unset, allowing insert() to add them naturally from index.json.
     */
    TEST_CASE("PackageFetcher::write_repodata_record backfills noarch")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/noarch/noarchpkg-1.0-pyhd8ed1ab_0.conda";
        auto pkg_info = specs::PackageInfo::from_url(url).value();

        const std::string pkg_basename = "noarchpkg-1.0-pyhd8ed1ab_0";

        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        auto pkg_extract_dir = cache_subdir / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        // Create index.json with noarch field
        nlohmann::json index_json;
        index_json["name"] = "noarchpkg";
        index_json["version"] = "1.0";
        index_json["build"] = "pyhd8ed1ab_0";
        index_json["noarch"] = "python";

        {
            std::ofstream index_file((info_dir / "index.json").std_path());
            index_file << index_json.dump(2);
        }

        {
            std::ofstream paths_file((info_dir / "paths.json").std_path());
            paths_file << R"({"paths": [], "paths_version": 1})";
        }

        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_basename + ".tar.bz2";

        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        REQUIRE(fs::exists(repodata_record_path));

        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        // noarch should be backfilled from index.json
        REQUIRE(repodata_record.contains("noarch"));
        CHECK(repodata_record["noarch"] == "python");
    }

    /**
     * Test that size is filled from tarball when zero
     *
     * PURPOSE: Verify existing size handling behavior continues to work.
     */
    TEST_CASE("PackageFetcher::write_repodata_record fills size from tarball")
    {
        auto& ctx = mambatests::context();
        TemporaryDirectory temp_dir;
        MultiPackageCache package_caches{ { temp_dir.path() / "pkgs" }, ctx.validation_params };

        static constexpr std::string_view url = "https://conda.anaconda.org/conda-forge/linux-64/sizepkg-1.0-h0_0.conda";
        auto pkg_info = specs::PackageInfo::from_url(url).value();

        // Verify precondition: size is 0 from URL parsing
        REQUIRE(pkg_info.size == 0);

        const std::string pkg_basename = "sizepkg-1.0-h0_0";

        const auto cache_subdir = temp_dir.path() / "pkgs"
                                  / package_cache_folder_relative_path(pkg_info);

        auto pkg_extract_dir = cache_subdir / pkg_basename;
        auto info_dir = pkg_extract_dir / "info";
        fs::create_directories(info_dir);

        nlohmann::json index_json;
        index_json["name"] = "sizepkg";
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

        auto tarball_path = cache_subdir / (pkg_basename + ".tar.bz2");
        create_archive(pkg_extract_dir, tarball_path, compression_algorithm::bzip2, 1, 1, nullptr);
        REQUIRE(fs::exists(tarball_path));

        auto tarball_size = fs::file_size(tarball_path);
        REQUIRE(tarball_size > 0);

        auto modified_pkg_info = pkg_info;
        modified_pkg_info.filename = pkg_basename + ".tar.bz2";

        fs::remove_all(pkg_extract_dir);

        PackageFetcher pkg_fetcher{ modified_pkg_info, package_caches };

        ExtractOptions options;
        options.sparse = false;
        options.subproc_mode = extract_subproc_mode::mamba_package;

        bool extract_success = pkg_fetcher.extract(options);
        REQUIRE(extract_success);

        auto repodata_record_path = pkg_extract_dir / "info" / "repodata_record.json";
        std::ifstream repodata_file(repodata_record_path.std_path());
        nlohmann::json repodata_record;
        repodata_file >> repodata_record;

        // Size should be filled from tarball
        REQUIRE(repodata_record.contains("size"));
        CHECK(repodata_record["size"] == tarball_size);
    }
}
