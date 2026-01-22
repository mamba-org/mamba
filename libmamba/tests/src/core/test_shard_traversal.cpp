// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/shard_loader.hpp"
#include "mamba/core/shard_traversal.hpp"
#include "mamba/core/shard_types.hpp"

using namespace mamba;

TEST_CASE("NodeId operations", "[mamba::core][mamba::core::shard_traversal]")
{
    SECTION("Equality")
    {
        NodeId id1{ "package", "channel", "url" };
        NodeId id2{ "package", "channel", "url" };
        NodeId id3{ "package2", "channel", "url" };

        REQUIRE(id1 == id2);
        REQUIRE_FALSE(id1 == id3);
    }

    SECTION("Comparison")
    {
        NodeId id1{ "a", "channel", "url" };
        NodeId id2{ "b", "channel", "url" };

        REQUIRE(id1 < id2);
    }
}

TEST_CASE("shard_mentioned_packages", "[mamba::core][mamba::core::shard_traversal]")
{
    SECTION("Extract dependencies from shard")
    {
        ShardDict shard;
        ShardPackageRecord record1;
        record1.name = "pkg1";
        record1.depends = { "dep1 >=1.0", "dep2" };
        shard.packages["pkg1-1.0.tar.bz2"] = record1;

        ShardPackageRecord record2;
        record2.name = "pkg2";
        record2.depends = { "dep1", "dep3" };
        shard.conda_packages["pkg2-2.0.conda"] = record2;

        auto mentioned = shard_mentioned_packages(shard);

        // Should extract package names from dependencies
        REQUIRE(mentioned.size() >= 2);  // At least dep1 and dep2/dep3
    }

    SECTION("Empty shard")
    {
        ShardDict empty_shard;
        auto mentioned = shard_mentioned_packages(empty_shard);
        REQUIRE(mentioned.empty());
    }
}
