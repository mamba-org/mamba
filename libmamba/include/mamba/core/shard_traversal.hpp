// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARD_TRAVERSAL_HPP
#define MAMBA_CORE_SHARD_TRAVERSAL_HPP

#include <climits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/shard_loader.hpp"
#include "mamba/core/shard_types.hpp"

namespace mamba
{
    // Forward declarations
    class Shards;

    /**
     * Node identifier for traversal graph.
     *
     * Uniquely identifies a (channel, package) tuple in the traversal.
     */
    struct NodeId
    {
        std::string package;
        std::string channel;
        std::string shard_url;

        bool operator==(const NodeId& other) const;
        bool operator<(const NodeId& other) const;
    };

    /**
     * Node in the dependency traversal graph.
     */
    struct Node
    {
        int distance = INT_MAX;
        std::string package;
        std::string channel;
        std::string shard_url;
        bool visited = false;

        NodeId to_id() const;
    };

    /**
     * Repodata subset builder using dependency traversal.
     *
     * Traverses dependencies of installed and to-be-installed packages to
     * generate a useful subset for the solver.
     */
    class RepodataSubset
    {
    public:

        /**
         * Create a repodata subset from shard-like objects.
         *
         * @param shardlikes Collection of ShardBase objects (Shards or ShardLike).
         */
        explicit RepodataSubset(std::vector<std::shared_ptr<ShardBase>> shardlikes);

        /**
         * Find all packages reachable from root packages.
         *
         * @param root_packages List of package names to start traversal from.
         * @param strategy Traversal strategy ("bfs" or "pipelined").
         * @param root_shardlike If provided, only create root nodes for packages in this shardlike
         *                       (ensures root packages are in the correct subdir)
         */
        auto reachable(
            const std::vector<std::string>& root_packages,
            const std::string& strategy = "pipelined",
            const std::shared_ptr<ShardBase>& root_shardlike = nullptr
        ) -> expected_t<void>;

        /**
         * Get all discovered nodes.
         */
        [[nodiscard]] auto nodes() const -> const std::map<NodeId, Node>&;

        /**
         * Get shard-like objects.
         */
        [[nodiscard]] auto shardlikes() const -> const std::vector<std::shared_ptr<ShardBase>>&;

    private:

        /**
         * Breadth-first search traversal.
         */
        auto reachable_bfs(
            const std::vector<std::string>& root_packages,
            const std::shared_ptr<ShardBase>& root_shardlike = nullptr
        ) -> expected_t<void>;

        /**
         * Pipelined traversal with concurrent cache/network fetching.
         */
        auto reachable_pipelined(
            const std::vector<std::string>& root_packages,
            const std::shared_ptr<ShardBase>& root_shardlike = nullptr
        ) -> expected_t<void>;

        /**
         * Get neighbors (dependencies) of a node.
         */
        auto neighbors(const Node& node) -> std::vector<Node>;

        /**
         * Extract package names from dependencies in a shard.
         */
        static auto extract_dependencies(const ShardDict& shard) -> std::vector<std::string>;

        /**
         * Visit a node and discover its dependencies.
         *
         * @param parent_node The parent node (distance 0 for root packages)
         * @param mentioned_packages Packages to create nodes for
         * @param restrict_to_shardlike If provided, only create nodes for packages in this
         * shardlike (used for root packages to ensure they're in the correct subdir)
         */
        auto visit_node(
            const Node& parent_node,
            const std::vector<std::string>& mentioned_packages,
            const std::shared_ptr<ShardBase>& restrict_to_shardlike = nullptr
        ) -> std::vector<NodeId>;

        /**
         * Drain pending nodes, separating those already loaded from those needing fetch.
         */
        auto drain_pending(
            std::set<NodeId>& pending,
            std::map<std::string, std::shared_ptr<ShardBase>>& shardlikes_by_url
        ) -> std::pair<std::vector<std::pair<NodeId, ShardDict>>, std::vector<NodeId>>;

        /** Shard-like objects. */
        std::vector<std::shared_ptr<ShardBase>> m_shardlikes;

        /** Discovered nodes. */
        std::map<NodeId, Node> m_nodes;
    };

    /**
     * Extract package names mentioned in a shard's dependencies.
     *
     * @param shard The shard to extract dependencies from.
     * @return Vector of unique package names.
     */
    auto shard_mentioned_packages(const ShardDict& shard) -> std::vector<std::string>;

}

#endif
