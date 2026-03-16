// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include <catch2/catch_all.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
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
}
