// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/shard_types.hpp"
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

        specs::RepoDataPackage pkg = to_repo_data_package(shard_record);

        REQUIRE(pkg.name == "test-package");
        REQUIRE(pkg.version.to_string() == "2.3.4");
        REQUIRE(pkg.build_string == "build456");
        REQUIRE(pkg.build_number == 100);
        REQUIRE(pkg.sha256 == "xyz789");
        REQUIRE(pkg.depends.size() == 1);
        REQUIRE(pkg.noarch == specs::NoArchType::Python);
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
    repodata_dict.packages["test-pkg-1.0.0.tar.bz2"] = record;

    specs::RepoData repo_data = to_repo_data(repodata_dict);

    REQUIRE(repo_data.version == 2);
    REQUIRE(repo_data.packages.size() == 1);
    REQUIRE(repo_data.packages.begin()->second.name == "test-pkg");
}
