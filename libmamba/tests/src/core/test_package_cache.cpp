// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"

#include "mambatests.hpp"

namespace mamba
{
    TEST_CASE("PackageCache")
    {
        auto& ctx = mambatests::context();
        // Disable safety checks for these tests because our manual setup is minimal
        // and doesn't contain 'paths.json', which would cause 'validate()' to fail.
        auto params = ctx.validation_params;
        params.safety_checks = VerificationLevel::Disabled;

        TemporaryDirectory temp_dir;
        fs::u8path pkgs_dir = temp_dir.path() / "pkgs";
        fs::create_directories(pkgs_dir);

        specs::PackageInfo pkg_info;
        pkg_info.name = "test-pkg";
        pkg_info.version = "1.0.0";
        pkg_info.build_string = "h123456_0";
        pkg_info.filename = "test-pkg-1.0.0-h123456_0.tar.bz2";
        pkg_info.channel = "conda-forge";
        pkg_info.platform = "noarch";
        pkg_info.package_url = "https://conda.anaconda.org/conda-forge/noarch/test-pkg-1.0.0-h123456_0.tar.bz2";
        pkg_info.size = 100;
        pkg_info.md5 = "d41d8cd98f00b204e9800998ecf8427e";  // md5 of empty string

        auto write_repodata_record = [](const fs::u8path& path, const specs::PackageInfo& s)
        {
            fs::create_directories(path.parent_path());
            nlohmann::json j;
            j["name"] = s.name;
            j["version"] = s.version;
            j["build"] = s.build_string;
            j["build_number"] = 0;
            j["size"] = s.size;
            j["md5"] = s.md5;
            j["url"] = s.package_url;
            j["channel"] = s.channel;
            std::ofstream f(path.std_path());
            f << j.dump();
        };

        auto write_empty_file = [](const fs::u8path& path)
        {
            fs::create_directories(path.parent_path());
            std::ofstream f(path.std_path());
        };

        const auto rel_path = package_cache_folder_relative_path(pkg_info);
        const fs::u8path hierarchical_dir = pkgs_dir / rel_path / "test-pkg-1.0.0-h123456_0";
        const fs::u8path flat_dir = pkgs_dir / "test-pkg-1.0.0-h123456_0";

        SECTION("Find package in flat cache")
        {
            write_repodata_record(flat_dir / "info" / "repodata_record.json", pkg_info);

            MultiPackageCache cache({ pkgs_dir }, params);
            REQUIRE(cache.get_extracted_dir_path(pkg_info) == pkgs_dir);
        }

        SECTION("Find package in hierarchical cache")
        {
            write_repodata_record(hierarchical_dir / "info" / "repodata_record.json", pkg_info);

            MultiPackageCache cache({ pkgs_dir }, params);
            REQUIRE(cache.get_extracted_dir_path(pkg_info) == pkgs_dir / rel_path);
        }

        SECTION("Prefer hierarchical cache if both exist")
        {
            write_repodata_record(flat_dir / "info" / "repodata_record.json", pkg_info);
            write_repodata_record(hierarchical_dir / "info" / "repodata_record.json", pkg_info);

            MultiPackageCache cache({ pkgs_dir }, params);
            REQUIRE(cache.get_extracted_dir_path(pkg_info) == pkgs_dir / rel_path);
        }

        SECTION("Fallback to flat if hierarchical is empty/invalid")
        {
            // Create the hierarchical directory but NO repodata_record.json inside it
            fs::create_directories(hierarchical_dir / "info");

            // Create valid flat cache
            write_repodata_record(flat_dir / "info" / "repodata_record.json", pkg_info);

            MultiPackageCache cache({ pkgs_dir }, params);
            // It must bypass the empty hierarchical dir and find the flat one
            REQUIRE(cache.get_extracted_dir_path(pkg_info) == pkgs_dir);
        }

        SECTION("Tarball: Find in flat cache")
        {
            write_empty_file(pkgs_dir / pkg_info.filename);

            MultiPackageCache cache({ pkgs_dir }, params);
            // We need to set size to 0 to bypass validation for empty file
            specs::PackageInfo pkg_no_val = pkg_info;
            pkg_no_val.size = 0;

            REQUIRE(cache.get_tarball_path(pkg_no_val) == pkgs_dir);
        }

        SECTION("Tarball: Find in hierarchical cache")
        {
            write_empty_file(pkgs_dir / rel_path / pkg_info.filename);

            MultiPackageCache cache({ pkgs_dir }, params);
            specs::PackageInfo pkg_no_val = pkg_info;
            pkg_no_val.size = 0;

            REQUIRE(cache.get_tarball_path(pkg_no_val) == pkgs_dir / rel_path);
        }

        SECTION("Tarball: Prefer hierarchical cache if both exist")
        {
            write_empty_file(pkgs_dir / rel_path / pkg_info.filename);
            write_empty_file(pkgs_dir / pkg_info.filename);

            MultiPackageCache cache({ pkgs_dir }, params);
            specs::PackageInfo pkg_no_val = pkg_info;
            pkg_no_val.size = 0;

            REQUIRE(cache.get_tarball_path(pkg_no_val) == pkgs_dir / rel_path);
        }

        SECTION("Tarball: Fallback to flat if hierarchical is missing")
        {
            // Create directory but no file inside it
            fs::create_directories(pkgs_dir / rel_path);

            write_empty_file(pkgs_dir / pkg_info.filename);

            MultiPackageCache cache({ pkgs_dir }, params);
            specs::PackageInfo pkg_no_val = pkg_info;
            pkg_no_val.size = 0;

            REQUIRE(cache.get_tarball_path(pkg_no_val) == pkgs_dir);
        }

        SECTION("Hierarchical cache under a long path")
        {
            constexpr std::size_t min_pkgs_dir_path_length = 300;
            constexpr auto segment = "very_long_directory_name_segment";

            fs::u8path long_pkgs_dir = temp_dir.path();
            while ((long_pkgs_dir / segment / "pkgs").generic_string().size()
                   <= min_pkgs_dir_path_length)
            {
                long_pkgs_dir /= segment;
            }
            long_pkgs_dir /= "pkgs";
            fs::create_directories(long_pkgs_dir);

            const fs::u8path long_hierarchical_dir = long_pkgs_dir / rel_path
                                                     / "test-pkg-1.0.0-h123456_0";
            write_repodata_record(long_hierarchical_dir / "info" / "repodata_record.json", pkg_info);

            MultiPackageCache cache({ long_pkgs_dir }, params);
            REQUIRE(cache.get_extracted_dir_path(pkg_info) == long_pkgs_dir / rel_path);
        }
    }
}  // namespace mamba
