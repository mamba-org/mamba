// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_GRAPH_HPP
#define MAMBA_PROBLEMS_GRAPH_HPP

#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>

#include "property_graph.hpp"
#include "mamba/core/package_info.hpp"
#include "pool.hpp"
#include "solver.hpp"


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

    class MProblemsGraphs
    {
    public: 
        using initial_conflict_graph = MPropertyGraph<MNode, MEdgeInfo>;
        using merged_conflict_graph = MPropertyGraph<MGroupNode, MGroupEdgeInfo>;
        using node_id = initial_conflict_graph::node_id;
        using group_node_id = merged_conflict_graph::node_id;
        
        MProblemsGraphs(int a);
        MProblemsGraphs(MPool* pool, const std::vector<MSolverProblem>& problems);
    
        void add_conflict_edge(MNode from_node, MNode to_node, std::string info);
        node_id get_or_create_node(MNode mnode);
        void add_solvables_to_conflicts(node_id source_node, node_id target_node);
        const Union<node_id>& create_unions();
        void create_merged_graph();
    private:
        MPool m_pool;
        Union<node_id> m_union;

        initial_conflict_graph m_initial_conflict_graph;
        merged_conflict_graph m_merged_conflict_graph;
        std::unordered_map<node_id, std::unordered_set<node_id>> solvables_to_conflicts;
        std::unordered_map<MNode, node_id, MNode::HashFunction> m_node_to_id;;

        void create_initial_graph(MPool* pool, const std::vector<MSolverProblem>& problems);
        void add_problem_to_graph(MPool* pool, const MSolverProblem& problem);

        std::unordered_map<node_id, group_node_id> create_merged_nodes();
        std::optional<std::string> get_package_name(node_id id);
    };

    inline std::size_t hash_vec(const std::unordered_set<MProblemsGraphs::node_id>& vec) noexcept
    {
        std::size_t seed = vec.size();
        for(auto& i : vec) {
            seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }

}

#endif //MAMBA_PROBLEMS_GRAPH_HPP
