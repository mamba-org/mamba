// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_GRAPH_HPP
#define MAMBA_PROBLEMS_GRAPH_HPP

#include <string>
#include <utility>
#include <variant>
#include <unordered_map>
#include <optional>

#include <solv/solver.h>

#include "mamba/core/util_graph.hpp"
#include "mamba/core/package_info.hpp"

namespace mamba
{

    class MSolver;
    class MPool;

    /**
     * Separate a dependency spec into a package name and the version range.
     */
    class DependencyInfo
    {
    public:
        DependencyInfo(const std::string& dependency);

        const std::string& name() const;
        const std::string& version_range() const;
        const std::string& build_range() const;
        std::string str() const;

    private:
        std::string m_name;
        std::string m_version_range;
        std::string m_build_range;
    };

    /**
     * A directed graph of the pacakges involved in a libsolv conflict.
     */
    class ProblemsGraph
    {
    public:
        struct RootNode
        {
        };
        struct PackageNode
        {
            PackageInfo package_info;
            std::optional<SolverRuleinfo> problem_type;
        };
        struct UnresolvedDependencyNode
        {
            std::string dependency;
            static SolverRuleinfo constexpr problem_type = SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP;
        };
        struct ConstraintNode
        {
            std::string dependency;
            static SolverRuleinfo constexpr problem_type = SOLVER_RULE_PKG_CONSTRAINS;
        };
        using node_t
            = std::variant<RootNode, PackageNode, UnresolvedDependencyNode, ConstraintNode>;

        using edge_t = DependencyInfo;

        using graph_t = DiGraph<node_t, edge_t>;
        using node_id = graph_t::node_id;
        using conflict_map = std::unordered_map<node_id, vector_set<node_id>>;

        static ProblemsGraph from_solver(MSolver const& solver, MPool const& pool);

        ProblemsGraph(graph_t graph, conflict_map conflicts, node_id root_node);

        graph_t const& graph() const noexcept;
        conflict_map const& conflicts() const noexcept;
        node_id root_node() const noexcept;

    private:
        graph_t m_graph;
        conflict_map m_conflicts;
        node_id m_root_node;
    };
}

#endif  // MAMBA_PROBLEMS_GRAPH_HPP
