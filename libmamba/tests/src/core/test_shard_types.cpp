// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/shard_types.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/specs/version.hpp"

using namespace mamba;

TEST_CASE("RepodataDict to RepoData conversion", "[mamba::core][mamba::core::shard_types]")
{
    RepodataDict repodata_dict;
    repodata_dict.info.base_url = "https://example.com/packages";
    repodata_dict.info.shards_base_url = "https://example.com/shards";
    repodata_dict.info.subdir = "linux-64";
    repodata_dict.repodata_version = 2;

    specs::RepoDataPackage record;
    record.name = "test-pkg";
    record.version = specs::Version::parse("1.0.0").value();
    repodata_dict.shard_dict.packages["test-pkg-1.0.0.tar.bz2"] = record;

    specs::RepoData repo_data = to_repo_data(repodata_dict);

    REQUIRE(repo_data.version == 2);
    REQUIRE(repo_data.packages.size() == 1);
    REQUIRE(repo_data.packages.begin()->second.name == "test-pkg");
}

TEST_CASE("to_package_info conversion", "[mamba::core][mamba::core::shard_types]")
{
    SECTION("Basic conversion with all fields")
    {
        specs::RepoDataPackage record;
        record.name = "test-package";
        record.version = specs::Version::parse("1.2.3").value();
        record.build_string = "py310_0";
        record.build_number = 42;
        record.sha256 = "abc123sha256";
        record.md5 = "def456md5";
        record.depends = { "python >=3.10", "numpy >=1.20" };
        record.constrains = { "scipy <2.0" };
        record.noarch = specs::NoArchType::Python;
        record.size = 98765;
        record.license = "MIT";
        record.license_family = "MIT";
        record.subdir = "noarch";
        record.timestamp = 1640995200;

        std::string filename = "test-package-1.2.3-py310_0.tar.bz2";
        std::string channel_id = "conda-forge";
        specs::DynamicPlatform platform = "linux-64";
        std::string base_url = "https://conda.anaconda.org/conda-forge/linux-64";

        specs::PackageInfo pkg_info = to_package_info(record, filename, channel_id, platform, base_url);

        REQUIRE(pkg_info.name == "test-package");
        REQUIRE(pkg_info.version == "1.2.3");
        REQUIRE(pkg_info.build_string == "py310_0");
        REQUIRE(pkg_info.build_number == 42);
        REQUIRE(pkg_info.sha256 == "abc123sha256");
        REQUIRE(pkg_info.md5 == "def456md5");
        REQUIRE(pkg_info.dependencies == record.depends);
        REQUIRE(pkg_info.constrains == record.constrains);
        REQUIRE(pkg_info.noarch == specs::NoArchType::Python);
        REQUIRE(pkg_info.size == 98765);
        REQUIRE(pkg_info.license == "MIT");
        REQUIRE(pkg_info.timestamp == 1640995200);
        REQUIRE(pkg_info.filename == filename);
        REQUIRE(pkg_info.channel == channel_id);
        REQUIRE(pkg_info.platform == platform);
        REQUIRE(
            pkg_info.package_url
            == "https://conda.anaconda.org/conda-forge/linux-64/test-package-1.2.3-py310_0.tar.bz2"
        );
    }

    SECTION("Conversion with generic noarch")
    {
        specs::RepoDataPackage record;
        record.name = "generic-pkg";
        record.version = specs::Version::parse("1.0.0").value();
        record.build_string = "0";
        record.noarch = specs::NoArchType::Generic;

        specs::PackageInfo pkg_info = to_package_info(
            record,
            "generic-pkg-1.0.0-0.tar.bz2",
            "conda-forge",
            "noarch",
            "https://conda.anaconda.org/conda-forge/noarch"
        );

        REQUIRE(pkg_info.noarch == specs::NoArchType::Generic);
    }

    SECTION("Conversion without noarch")
    {
        specs::RepoDataPackage record;
        record.name = "native-pkg";
        record.version = specs::Version::parse("2.0.0").value();
        record.build_string = "h123_1";
        // noarch is not set

        specs::PackageInfo pkg_info = to_package_info(
            record,
            "native-pkg-2.0.0-h123_1.conda",
            "conda-forge",
            "linux-64",
            "https://conda.anaconda.org/conda-forge/linux-64"
        );

        REQUIRE(pkg_info.noarch == specs::NoArchType::No);
    }

    SECTION("Conversion without optional hashes")
    {
        specs::RepoDataPackage record;
        record.name = "no-hash-pkg";
        record.version = specs::Version::parse("3.0.0").value();
        record.build_string = "0";
        // sha256 and md5 are not set

        specs::PackageInfo pkg_info = to_package_info(
            record,
            "no-hash-pkg-3.0.0-0.tar.bz2",
            "test-channel",
            "osx-64",
            "https://example.com/test-channel/osx-64"
        );

        REQUIRE(pkg_info.sha256.empty());
        REQUIRE(pkg_info.md5.empty());
    }

    SECTION("Conversion with optional metadata fields")
    {
        specs::RepoDataPackage record;
        record.name = "metadata-pkg";
        record.version = specs::Version::parse("1.0.0").value();
        record.build_string = "0";
        record.license = "BSD-3-Clause";
        record.license_family = "BSD";
        record.subdir = "linux-64";
        record.timestamp = 1234567890;

        specs::PackageInfo pkg_info = to_package_info(
            record,
            "metadata-pkg-1.0.0-0.tar.bz2",
            "channel",
            "linux-64",
            "https://example.com/channel/linux-64"
        );

        REQUIRE(pkg_info.license == "BSD-3-Clause");
        REQUIRE(pkg_info.timestamp == 1234567890);
    }

    SECTION("Conversion without optional metadata fields")
    {
        specs::RepoDataPackage record;
        record.name = "minimal-pkg";
        record.version = specs::Version::parse("1.0.0").value();
        record.build_string = "0";
        // license, license_family, subdir, timestamp are not set

        specs::PackageInfo pkg_info = to_package_info(
            record,
            "minimal-pkg-1.0.0-0.tar.bz2",
            "channel",
            "linux-64",
            "https://example.com/channel/linux-64"
        );

        REQUIRE(pkg_info.license.empty());
        REQUIRE(pkg_info.timestamp == 0);
    }

    SECTION("URL construction with trailing slash in base_url")
    {
        specs::RepoDataPackage record;
        record.name = "url-test-pkg";
        record.version = specs::Version::parse("1.0.0").value();
        record.build_string = "0";

        // Base URL with trailing slash
        specs::PackageInfo pkg_info = to_package_info(
            record,
            "url-test-pkg-1.0.0-0.tar.bz2",
            "channel",
            "win-64",
            "https://example.com/channel/win-64/"
        );

        // Should not have double slashes
        REQUIRE(
            pkg_info.package_url == "https://example.com/channel/win-64/url-test-pkg-1.0.0-0.tar.bz2"
        );
    }

    SECTION("Conversion with empty dependencies and constrains")
    {
        specs::RepoDataPackage record;
        record.name = "no-deps-pkg";
        record.version = specs::Version::parse("1.0.0").value();
        record.build_string = "0";
        // depends and constrains are empty by default

        specs::PackageInfo pkg_info = to_package_info(
            record,
            "no-deps-pkg-1.0.0-0.tar.bz2",
            "channel",
            "linux-64",
            "https://example.com"
        );

        REQUIRE(pkg_info.dependencies.empty());
        REQUIRE(pkg_info.constrains.empty());
    }
}
