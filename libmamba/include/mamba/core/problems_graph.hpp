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
#include <tuple>
#include <functional>
#include <iostream>

#include "mamba/core/solver_problems.hpp"
#include "mamba/core/property_graph.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/util.hpp"

extern "C"
{
#include "solv/solver.h"
}

namespace mamba
{
    class MNode
    {
    public:
        MNode(const PackageInfo& package_info,
              std::optional<SolverRuleinfo> problem_type = std::nullopt);
        MNode(std::string dep, std::optional<SolverRuleinfo> problem_type = std::nullopt);
        MNode();

        std::optional<PackageInfo> m_package_info;
        std::optional<std::string> m_dep;
        std::optional<SolverRuleinfo> m_problem_type;
        bool m_is_root;

        std::string get_name() const;
        bool is_root() const;
        bool is_conflict() const;

        bool operator==(const MNode& node) const;

        struct HashFunction
        {
            size_t operator()(const MNode& node) const
            {
                if (node.m_package_info)
                {
                    return PackageInfoHash()(node.m_package_info.value());
                }
                else if (node.m_dep)
                {
                    return std::hash<std::string>()(node.m_dep.value());
                }
                // root
                return 0;
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
        bool m_is_root;
        std::optional<std::string> m_dep;
        std::optional<std::string> m_pkg_name;
        std::unordered_set<std::string> m_pkg_versions;
        std::optional<SolverRuleinfo> m_problem_type;

        MGroupNode(const MNode& node);
        MGroupNode();

        void add(const MNode& node);
        std::string get_name() const;
        bool is_root() const;
        bool is_conflict() const;
        bool requested() const;  // TODO maybe not needed
    };

    class MGroupEdgeInfo
    {
    public:
        std::set<std::string> m_deps;

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
        using conflicts_node_ids = std::unordered_map<node_id, std::set<node_id>>;
        using conflicts_group_ids = std::unordered_map<group_node_id, std::set<group_node_id>>;

        MProblemsGraphs();
        MProblemsGraphs(MPool* pool);
        MProblemsGraphs(MPool* pool,
                        const std::vector<MSolverProblem>& problems,
                        const std::vector<std::string>& initial_specs);

        merged_conflict_graph create_graph(const std::vector<MSolverProblem>& problems,
                                           const std::vector<std::string>& initial_specs);

        void add_conflict_edge(MNode from_node, MNode to_node, std::string info);
        node_id get_or_create_node(MNode mnode);
        void add_solvables_to_conflicts(node_id source_node, node_id target_node);
        void create_unions();

        const conflicts_group_ids& get_groups_conflicts();
        merged_conflict_graph create_merged_graph();

        UnionFind<node_id> m_union;
        initial_conflict_graph m_initial_conflict_graph;
        merged_conflict_graph m_merged_conflict_graph;

        conflicts_node_ids solvables_to_conflicts;
        conflicts_group_ids group_solvables_to_conflicts;

    private:
        MPool* m_pool;

        std::unordered_map<MNode, node_id, MNode::HashFunction> m_node_to_id;

        void create_initial_graph(const std::vector<MSolverProblem>& problems,
                                  const std::vector<std::string>& initial_specs);
        void add_problem_to_graph(const MSolverProblem& problem,
                                  const std::vector<std::string>& initial_specs);

        std::unordered_map<node_id, group_node_id> create_merged_nodes();
        std::optional<std::string> get_package_name(node_id id);
    };

    inline std::ostream& operator<<(std::ostream& strm, const MGroupNode& node)
    {
        if (node.m_pkg_name)
        {
            return strm << "package " << node.m_pkg_name.value() << " versions ["
                        << join(node.m_pkg_versions) + "]";
        }
        else if (node.m_dep)
        {
            return strm << "No packages matching " << node.m_dep.value()
                        << " could be found in the provided channels";
        }
        else if (node.is_root())
        {
            return strm << "root";
        }
        return strm << "invalid";
    }

    inline std::ostream& operator<<(std::ostream& strm, const MGroupEdgeInfo& edge)
    {
        return strm << join(edge.m_deps);
    }

    inline std::string get_value_or(const std::optional<PackageInfo>& pkg_info)
    {
        if (pkg_info)
        {
            return pkg_info.value().str();
        }
        else
        {
            return "(null)";
        }
    }

    template <typename T>
    inline bool has_values(const MSolverProblem& problem,
                           std::initializer_list<std::optional<T>> args)
    {
        for (const auto& e : args)
        {
            if (!e)
            {
                LOG_WARNING << "Unexpected empty optionals for problem " << problem.to_string()
                            << "source: " << get_value_or(problem.source())
                            << "target: " << get_value_or(problem.target())
                            << "dep: " << problem.dep().value_or("(null)") << std::endl;
                return false;
            }
        }
        return true;
    }

    inline bool contains_any_substring(std::string str, const std::vector<std::string>& substrings)
    {
        for (const auto& substring : substrings)
        {
            if (str.find(substring) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }
}

#endif  // MAMBA_PROBLEMS_GRAPH_HPP
