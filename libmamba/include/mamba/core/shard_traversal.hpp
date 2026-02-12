// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARD_TRAVERSAL_HPP
#define MAMBA_CORE_SHARD_TRAVERSAL_HPP

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "mamba/core/shard_types.hpp"
#include "mamba/core/shards.hpp"

namespace mamba
{

    /**
     * Unique identifier for a node in the shard dependency graph.
     */
    struct NodeId
    {
        std::string package;
        std::string channel;
        std::string shard_url;

        [[nodiscard]] auto operator==(const NodeId& other) const -> bool;
        [[nodiscard]] auto operator<(const NodeId& other) const -> bool;
    };

    /**
     * A node in the shard dependency graph.
     */
    struct Node
    {
        std::size_t distance;
        std::string package;
        std::string channel;
        std::string shard_url;
        bool visited;

        [[nodiscard]] auto to_id() const -> NodeId;
    };

    using NodeMap = std::map<NodeId, Node>;
    using ShardsPtrVector = std::vector<std::shared_ptr<Shards>>;

    /**
     * Extracts package names from dependency specs in a shard.
     *
     * Parses depends and constrains via specs::MatchSpec, returning the
     * package names for dependency traversal.
     */
    [[nodiscard]] auto shard_mentioned_packages(const ShardDict& shard) -> std::vector<std::string>;

    /**
     * Traverses sharded repodata to find reachable packages from root packages.
     *
     * Uses BFS or pipelined strategy to explore dependencies across shards.
     */
    class RepodataSubset
    {
    public:

        explicit RepodataSubset(ShardsPtrVector shards);

        /**
         * Compute reachable packages from root_packages.
         *
         * @param root_packages Package names to start traversal from
         * @param strategy "bfs" or "pipelined"
         * @param root_shards If set, restrict root nodes to packages in these shard URLs
         */
        void reachable(
            const std::vector<std::string>& root_packages,
            const std::string& strategy = "bfs",
            std::optional<std::reference_wrapper<const std::set<std::string>>> root_shards = std::nullopt
        );

        /** Return visited nodes. */
        [[nodiscard]] auto nodes() const -> const NodeMap&;

        /** Return the shards collection. */
        [[nodiscard]] auto shards() const -> const ShardsPtrVector&;

    private:

        ShardsPtrVector m_shards;
        std::map<std::string, std::shared_ptr<Shards>> m_shards_by_url;
        NodeMap m_nodes;

        void reachable_bfs(
            const std::vector<std::string>& root_packages,
            std::optional<std::reference_wrapper<const std::set<std::string>>> root_shards
        );
        void reachable_pipelined(
            const std::vector<std::string>& root_packages,
            std::optional<std::reference_wrapper<const std::set<std::string>>> root_shards
        );
        void init_pending_with_roots(
            const std::vector<std::string>& root_packages,
            std::optional<std::reference_wrapper<const std::set<std::string>>> root_shards,
            std::vector<NodeId>& pending
        );
        void fetch_missing_shards_for_batch(const std::vector<NodeId>& batch);
        void process_bfs_batch(const std::vector<NodeId>& batch, std::vector<NodeId>& pending);
        void visit_node(const NodeId& node_id, std::vector<NodeId>& pending);
        void drain_pending(std::vector<NodeId>& pending);
        std::vector<NodeId> neighbors(const NodeId& node_id);
    };

}

#endif
