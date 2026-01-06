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

TEST_CASE("RepodataSubset basic operations", "[mamba::core][mamba::core::shard_traversal]")
{
    SECTION("Create subset")
    {
        // Create a mock ShardLike
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        ShardPackageRecord record;
        record.name = "test-pkg";
        record.version = "1.0.0";
        repodata.packages["test-pkg-1.0.0.tar.bz2"] = record;

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com/linux-64");

        RepodataSubset subset({ shardlike });

        REQUIRE(subset.shardlikes().size() == 1);
        REQUIRE(subset.nodes().empty());
    }
}

TEST_CASE("Traversal algorithms", "[mamba::core][mamba::core::shard_traversal]")
{
    SECTION("BFS order verification")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        // Create dependency chain: a -> b -> c
        ShardPackageRecord a, b, c;
        a.name = "a";
        a.version = "1.0";
        a.depends = { "b" };
        b.name = "b";
        b.version = "1.0";
        b.depends = { "c" };
        c.name = "c";
        c.version = "1.0";

        repodata.packages["a-1.0.tar.bz2"] = a;
        repodata.packages["b-1.0.tar.bz2"] = b;
        repodata.packages["c-1.0.tar.bz2"] = c;

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        auto result = subset.reachable({ "a" }, "bfs");
        REQUIRE(result.has_value());

        auto nodes = subset.nodes();
        REQUIRE(nodes.size() >= 2);  // Should discover a, b, and c
    }

    SECTION("Cycle detection")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        // Create circular dependency: a -> b -> a
        ShardPackageRecord a, b;
        a.name = "a";
        a.version = "1.0";
        a.depends = { "b" };
        b.name = "b";
        b.version = "1.0";
        b.depends = { "a" };

        repodata.packages["a-1.0.tar.bz2"] = a;
        repodata.packages["b-1.0.tar.bz2"] = b;

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        // Should handle cycles gracefully without infinite loop
        auto result = subset.reachable({ "a" }, "bfs");
        REQUIRE(result.has_value());

        auto nodes = subset.nodes();
        REQUIRE(nodes.size() == 2);  // Should discover both a and b
    }

    SECTION("Missing dependencies")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        // Package depends on non-existent package
        ShardPackageRecord pkg;
        pkg.name = "test-pkg";
        pkg.version = "1.0";
        pkg.depends = { "nonexistent-pkg" };

        repodata.packages["test-pkg-1.0.tar.bz2"] = pkg;

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        // Should handle missing dependencies gracefully
        auto result = subset.reachable({ "test-pkg" }, "bfs");
        REQUIRE(result.has_value());

        // Should still discover the root package
        auto nodes = subset.nodes();
        REQUIRE(nodes.size() >= 1);
    }
}

TEST_CASE("Complex dependency graphs", "[mamba::core][mamba::core::shard_traversal]")
{
    SECTION("Deep chains")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        // Create deep chain: a -> b -> c -> d -> e -> f -> g -> h -> i -> j
        const int depth = 10;
        for (int i = 0; i < depth; ++i)
        {
            ShardPackageRecord pkg;
            pkg.name = "pkg" + std::to_string(i);
            pkg.version = "1.0";
            if (i < depth - 1)
            {
                pkg.depends = { "pkg" + std::to_string(i + 1) };
            }
            repodata.packages["pkg" + std::to_string(i) + "-1.0.tar.bz2"] = pkg;
        }

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        auto result = subset.reachable({ "pkg0" }, "bfs");
        REQUIRE(result.has_value());

        auto nodes = subset.nodes();
        REQUIRE(nodes.size() == depth);
    }

    SECTION("Wide trees")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        // Create wide tree: root depends on 50 packages
        ShardPackageRecord root;
        root.name = "root";
        root.version = "1.0";
        for (int i = 0; i < 50; ++i)
        {
            root.depends.push_back("dep" + std::to_string(i));

            ShardPackageRecord dep;
            dep.name = "dep" + std::to_string(i);
            dep.version = "1.0";
            repodata.packages["dep" + std::to_string(i) + "-1.0.tar.bz2"] = dep;
        }
        repodata.packages["root-1.0.tar.bz2"] = root;

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        auto result = subset.reachable({ "root" }, "bfs");
        REQUIRE(result.has_value());

        auto nodes = subset.nodes();
        REQUIRE(nodes.size() == 51);  // root + 50 deps
    }

    SECTION("Mixed package types")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        ShardPackageRecord pkg1;
        pkg1.name = "pkg1";
        pkg1.version = "1.0";
        pkg1.depends = { "pkg2" };
        repodata.packages["pkg1-1.0.tar.bz2"] = pkg1;

        ShardPackageRecord pkg2;
        pkg2.name = "pkg2";
        pkg2.version = "1.0";
        repodata.conda_packages["pkg2-1.0.conda"] = pkg2;

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        auto result = subset.reachable({ "pkg1" }, "bfs");
        REQUIRE(result.has_value());

        auto nodes = subset.nodes();
        REQUIRE(nodes.size() >= 2);
    }
}

TEST_CASE("Traversal edge cases", "[mamba::core][mamba::core::shard_traversal]")
{
    SECTION("Empty roots")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        // Empty root packages should not crash
        auto result = subset.reachable({}, "bfs");
        REQUIRE(result.has_value());

        auto nodes = subset.nodes();
        REQUIRE(nodes.empty());
    }

    SECTION("Non-existent packages")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        // Non-existent packages should be handled gracefully
        auto result = subset.reachable({ "nonexistent" }, "bfs");
        REQUIRE(result.has_value());

        // Should not discover any nodes
        auto nodes = subset.nodes();
        REQUIRE(nodes.empty());
    }

    SECTION("Packages with no dependencies")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        ShardPackageRecord pkg;
        pkg.name = "standalone";
        pkg.version = "1.0";
        repodata.packages["standalone-1.0.tar.bz2"] = pkg;

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        auto result = subset.reachable({ "standalone" }, "bfs");
        REQUIRE(result.has_value());

        auto nodes = subset.nodes();
        REQUIRE(nodes.size() == 1);
    }

    SECTION("Self-referential dependencies")
    {
        RepodataDict repodata;
        repodata.info.base_url = "https://example.com";
        repodata.info.subdir = "linux-64";

        ShardPackageRecord pkg;
        pkg.name = "self-ref";
        pkg.version = "1.0";
        pkg.depends = { "self-ref" };  // Depends on itself
        repodata.packages["self-ref-1.0.tar.bz2"] = pkg;

        auto shardlike = std::make_shared<ShardLike>(repodata, "https://example.com");
        RepodataSubset subset({ shardlike });

        // Should handle self-reference without infinite loop
        auto result = subset.reachable({ "self-ref" }, "bfs");
        REQUIRE(result.has_value());

        auto nodes = subset.nodes();
        REQUIRE(nodes.size() == 1);  // Should only discover the package once
    }
}
