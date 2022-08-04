// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_EXPLAINER_HPP
#define MAMBA_PROBLEMS_EXPLAINER_HPP

#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <unordered_set>

#include "solver.hpp"
#include "problems_graph.hpp"
#include "package_info.hpp"
#include "pool.hpp"
#include "pool_wrapper.hpp"

extern "C"
{
#include "solv/queue.h"
#include "solv/solver.h"
}

namespace mamba
{
    class MNode
    {
    public:
        MNode(PackageInfo package_info);
        MNode(std::string dep);

        std::optional<PackageInfo> m_package_info;
        std::optional<std::string> m_dep;
        bool m_is_root;

        bool is_invalid_dep() const;
        bool is_root() const;
        bool operator==(const MNode& node) const;

        struct HashFunction
        {
            size_t operator()(const MNode& node) const
            {
                //TODO is this correct ?
                if (node.m_package_info.has_value()) {
                    return std::hash<std::string>()(node.m_package_info.value().sha256);
                }
                return std::hash<std::string>()(node.m_dep.value());
            }
        };
    };

    class MEdgeInfo
    {
    public:
        std::string m_dep;

        MEdgeInfo(std::string dep);
    };

    class MGroupNode
    {
    public:
        std::string m_pkg_name;
        std::vector<std::string> m_pkg_versions;
        bool m_is_root;

        MGroupNode(const MNode& node);
        MGroupNode();

        void add(const MNode& node);
        bool is_root() const;
    };

    class MGroupEdgeInfo
    {
    public:
        std::vector<std::string> m_deps;

        MGroupEdgeInfo(const MEdgeInfo& dep);

        void add(MEdgeInfo dep);
        bool operator==(const MGroupEdgeInfo& edge);
    };

    class MProblemsExplainer
    {
    public:
        using initial_conflict_graph = MGraph<MNode, MEdgeInfo>;
        using merged_conflict_graph = MGraph<MGroupNode, MGroupEdgeInfo>;
        using node_id = initial_conflict_graph::node_id;
        using group_node_id = merged_conflict_graph::node_id;

        MProblemsExplainer(IMPool* pool_wrapper, const std::vector<MSolverProblem>& problems);
        MProblemsExplainer();

        void add_conflict_edge(MNode from_node, MNode to_node, std::string info, std::unordered_map<MNode, node_id, MNode::HashFunction>& visited);
        node_id get_or_create_node(MNode mnode, std::unordered_map<MNode, node_id, MNode::HashFunction>& visited);
        void add_solvables_to_conflicts(node_id source_node, node_id target_node);
        std::string explain_conflicts();

    private:
        IMPool* m_pool;
        Union<node_id> m_union;

        initial_conflict_graph m_initial_conflict_graph;
        merged_conflict_graph m_merged_conflict_graph;
        std::unordered_map<node_id, std::unordered_set<node_id>> solvables_to_conflicts;
        
        void create_initial_graph(const std::vector<MSolverProblem>& problems);
        void add_problem_to_graph(const MSolverProblem& problem, std::unordered_map<MNode, node_id, MNode::HashFunction>& visited);

        std::unordered_map<node_id, group_node_id> create_merged_nodes();
        void create_merged_graph();

        std::optional<std::string> get_package_name(node_id id);

        std::string get_top_level_error() const;

        std::string explain(const std::vector<std::pair<MGroupNode, MGroupEdgeInfo>>& node_to_edge) const;
        std::string explain(const std::tuple<MGroupNode, MGroupEdgeInfo, MGroupEdgeInfo>& node_to_edge_to_req) const;
        
    };

    inline std::size_t hash_vec(const std::unordered_set<MProblemsExplainer::node_id>& vec) noexcept
    {
        std::size_t seed = vec.size();
        for(auto& i : vec) {
            seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }

    inline std::string join(std::vector<std::string> const &vec)
    {
        return "[" +  std::accumulate(vec.begin(), vec.end(), std::string(",")) + "]";
    }
}  // namespace mamba

#endif  // MAMBA_PROBLEMS_EXPLAINER_HPP