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

TEST_CASE("ShardPackageRecord conversion", "[mamba::core][mamba::core::shard_types]")
{
    SECTION("Convert RepoDataPackage to ShardPackageRecord")
    {
        specs::RepoDataPackage pkg;
        pkg.name = "test-package";
        pkg.version = specs::Version::parse("1.2.3").value();
        pkg.build_string = "build123";
        pkg.build_number = 42;
        pkg.sha256 = "abc123";
        pkg.md5 = "def456";
        pkg.depends = { "dep1", "dep2" };
        pkg.constrains = { "constraint1" };
        pkg.noarch = specs::NoArchType::Generic;
        pkg.license = "MIT";
        pkg.license_family = "MIT";
        pkg.subdir = "linux-64";
        pkg.timestamp = 1234567890;
        pkg.size = 98765;

        ShardPackageRecord shard_record = from_repo_data_package(pkg);

        REQUIRE(shard_record.name == "test-package");
        REQUIRE(shard_record.version == "1.2.3");
        REQUIRE(shard_record.build == "build123");
        REQUIRE(shard_record.build_number == 42);
        REQUIRE(shard_record.sha256 == "abc123");
        REQUIRE(shard_record.md5 == "def456");
        REQUIRE(shard_record.depends.size() == 2);
        REQUIRE(shard_record.constrains.size() == 1);
        REQUIRE(shard_record.noarch == "generic");
        REQUIRE(shard_record.license == "MIT");
        REQUIRE(shard_record.license_family == "MIT");
        REQUIRE(shard_record.subdir == "linux-64");
        REQUIRE(shard_record.timestamp == 1234567890);
        REQUIRE(shard_record.size == 98765);
    }

    SECTION("Convert ShardPackageRecord to RepoDataPackage")
    {
        ShardPackageRecord shard_record;
        shard_record.name = "test-package";
        shard_record.version = "2.3.4";
        shard_record.build = "build456";
        shard_record.build_number = 100;
        shard_record.sha256 = "xyz789";
        shard_record.depends = { "dep3" };
        shard_record.noarch = "python";
        shard_record.license = "BSD";
        shard_record.license_family = "BSD";
        shard_record.subdir = "noarch";
        shard_record.timestamp = 9876543210;
        shard_record.size = 54321;

        specs::RepoDataPackage pkg = to_repo_data_package(shard_record);

        REQUIRE(pkg.name == "test-package");
        REQUIRE(pkg.version.to_string() == "2.3.4");
        REQUIRE(pkg.build_string == "build456");
        REQUIRE(pkg.build_number == 100);
        REQUIRE(pkg.sha256 == "xyz789");
        REQUIRE(pkg.depends.size() == 1);
        REQUIRE(pkg.noarch == specs::NoArchType::Python);
        REQUIRE(pkg.license == "BSD");
        REQUIRE(pkg.license_family == "BSD");
        REQUIRE(pkg.subdir == "noarch");
        REQUIRE(pkg.timestamp == 9876543210);
        REQUIRE(pkg.size == 54321);
    }
}

TEST_CASE("RepodataDict to RepoData conversion", "[mamba::core][mamba::core::shard_types]")
{
    RepodataDict repodata_dict;
    repodata_dict.info.base_url = "https://example.com/packages";
    repodata_dict.info.shards_base_url = "https://example.com/shards";
    repodata_dict.info.subdir = "linux-64";
    repodata_dict.repodata_version = 2;

    ShardPackageRecord record;
    record.name = "test-pkg";
    record.version = "1.0.0";
    repodata_dict.shard_dict.packages["test-pkg-1.0.0.tar.bz2"] = record;

    specs::RepoData repo_data = to_repo_data(repodata_dict);

    REQUIRE(repo_data.version == 2);
    REQUIRE(repo_data.packages.size() == 1);
    REQUIRE(repo_data.packages.begin()->second.name == "test-pkg");
}

TEST_CASE("ShardPackageRecord round-trip conversion", "[mamba::core][mamba::core::shard_types]")
{
    SECTION("ShardPackageRecord -> RepoDataPackage -> ShardPackageRecord")
    {
        ShardPackageRecord original;
        original.name = "roundtrip-package";
        original.version = "3.2.1";
        original.build = "py39_0";
        original.build_number = 5;
        original.sha256 = "abcdef1234567890";
        original.md5 = "1234567890abcdef";
        original.depends = { "python >=3.9", "numpy" };
        original.constrains = { "scipy <2.0" };
        original.noarch = "python";
        original.size = 12345;
        original.license = "Apache-2.0";
        original.license_family = "Apache";
        original.subdir = "linux-64";
        original.timestamp = 1609459200;

        // Convert to RepoDataPackage and back
        specs::RepoDataPackage repo_pkg = to_repo_data_package(original);
        ShardPackageRecord roundtripped = from_repo_data_package(repo_pkg);

        // Verify all fields are preserved
        REQUIRE(roundtripped.name == original.name);
        REQUIRE(roundtripped.version == original.version);
        REQUIRE(roundtripped.build == original.build);
        REQUIRE(roundtripped.build_number == original.build_number);
        REQUIRE(roundtripped.sha256 == original.sha256);
        REQUIRE(roundtripped.md5 == original.md5);
        REQUIRE(roundtripped.depends == original.depends);
        REQUIRE(roundtripped.constrains == original.constrains);
        REQUIRE(roundtripped.noarch == original.noarch);
        REQUIRE(roundtripped.size == original.size);
        REQUIRE(roundtripped.license == original.license);
        REQUIRE(roundtripped.license_family == original.license_family);
        REQUIRE(roundtripped.subdir == original.subdir);
        REQUIRE(roundtripped.timestamp == original.timestamp);
    }

    SECTION("RepoDataPackage -> ShardPackageRecord -> RepoDataPackage")
    {
        specs::RepoDataPackage original;
        original.name = "roundtrip-pkg";
        original.version = specs::Version::parse("4.5.6").value();
        original.build_string = "h123abc_1";
        original.build_number = 10;
        original.sha256 = "sha256hash";
        original.md5 = "md5hash";
        original.depends = { "libstdcxx-ng >=7.5.0", "openssl >=1.1.1" };
        original.constrains = { "some-constraint >=1.0" };
        original.noarch = specs::NoArchType::Generic;
        original.license = "GPL-3.0";
        original.license_family = "GPL";
        original.subdir = "osx-64";
        original.timestamp = 1704067200;
        original.size = 45678;

        // Convert to ShardPackageRecord and back
        ShardPackageRecord shard_rec = from_repo_data_package(original);
        specs::RepoDataPackage roundtripped = to_repo_data_package(shard_rec);

        // Verify all fields are preserved
        REQUIRE(roundtripped.name == original.name);
        REQUIRE(roundtripped.version.to_string() == original.version.to_string());
        REQUIRE(roundtripped.build_string == original.build_string);
        REQUIRE(roundtripped.build_number == original.build_number);
        REQUIRE(roundtripped.sha256 == original.sha256);
        REQUIRE(roundtripped.md5 == original.md5);
        REQUIRE(roundtripped.depends == original.depends);
        REQUIRE(roundtripped.constrains == original.constrains);
        REQUIRE(roundtripped.noarch == original.noarch);
        REQUIRE(roundtripped.license == original.license);
        REQUIRE(roundtripped.license_family == original.license_family);
        REQUIRE(roundtripped.subdir == original.subdir);
        REQUIRE(roundtripped.timestamp == original.timestamp);
        REQUIRE(roundtripped.size == original.size);
    }

    SECTION("Round-trip with no noarch")
    {
        ShardPackageRecord original;
        original.name = "no-noarch-pkg";
        original.version = "1.0.0";
        original.build = "build_0";
        // noarch is not set (nullopt)

        specs::RepoDataPackage repo_pkg = to_repo_data_package(original);
        ShardPackageRecord roundtripped = from_repo_data_package(repo_pkg);

        REQUIRE_FALSE(roundtripped.noarch.has_value());
    }

    SECTION("Round-trip with generic noarch")
    {
        ShardPackageRecord original;
        original.name = "generic-noarch-pkg";
        original.version = "2.0.0";
        original.build = "build_1";
        original.noarch = "generic";

        specs::RepoDataPackage repo_pkg = to_repo_data_package(original);
        ShardPackageRecord roundtripped = from_repo_data_package(repo_pkg);

        REQUIRE(roundtripped.noarch == "generic");
    }

    SECTION("Round-trip without optional metadata fields")
    {
        ShardPackageRecord original;
        original.name = "minimal-metadata-pkg";
        original.version = "1.0.0";
        original.build = "0";
        // license, license_family, subdir, timestamp are not set

        specs::RepoDataPackage repo_pkg = to_repo_data_package(original);
        ShardPackageRecord roundtripped = from_repo_data_package(repo_pkg);

        REQUIRE_FALSE(roundtripped.license.has_value());
        REQUIRE_FALSE(roundtripped.license_family.has_value());
        REQUIRE_FALSE(roundtripped.subdir.has_value());
        REQUIRE_FALSE(roundtripped.timestamp.has_value());
    }
}

TEST_CASE("to_package_info conversion", "[mamba::core][mamba::core::shard_types]")
{
    SECTION("Basic conversion with all fields")
    {
        ShardPackageRecord record;
        record.name = "test-package";
        record.version = "1.2.3";
        record.build = "py310_0";
        record.build_number = 42;
        record.sha256 = "abc123sha256";
        record.md5 = "def456md5";
        record.depends = { "python >=3.10", "numpy >=1.20" };
        record.constrains = { "scipy <2.0" };
        record.noarch = "python";
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
        ShardPackageRecord record;
        record.name = "generic-pkg";
        record.version = "1.0.0";
        record.build = "0";
        record.noarch = "generic";

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
        ShardPackageRecord record;
        record.name = "native-pkg";
        record.version = "2.0.0";
        record.build = "h123_1";
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
        ShardPackageRecord record;
        record.name = "no-hash-pkg";
        record.version = "3.0.0";
        record.build = "0";
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
        ShardPackageRecord record;
        record.name = "metadata-pkg";
        record.version = "1.0.0";
        record.build = "0";
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
        ShardPackageRecord record;
        record.name = "minimal-pkg";
        record.version = "1.0.0";
        record.build = "0";
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
        ShardPackageRecord record;
        record.name = "url-test-pkg";
        record.version = "1.0.0";
        record.build = "0";

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
        ShardPackageRecord record;
        record.name = "no-deps-pkg";
        record.version = "1.0.0";
        record.build = "0";
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
