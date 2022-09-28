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

#include "mamba/core/graph_util.hpp"
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
        /**
         * A simplification of the libsolv SolverRuleinfo
         */
        enum class ProblemType
        {
            CONFLICT,
            NOT_FOUND,
            NOT_INSTALLABLE,
            BEST_NOT_INSTALLABLE,
            ONLY_DIRECT_INSTALL,
            EXCLUDED_BY_REPO_PRIORITY,
            INFERIOR_ARCH,
            PROVIDED_BY_SYSTEM
        };

        struct PackageNode
        {
            PackageInfo package_info;
            std::optional<ProblemType> problem_type;
        };
        struct UnresolvedDependencyNode
        {
            std::string dependency;
            std::optional<ProblemType> problem_type;
        };
        struct RootNode
        {
        };
        using node_t = std::variant<PackageNode, UnresolvedDependencyNode, RootNode>;

        struct RequireEdge : DependencyInfo
        {
        };
        struct ConstraintEdge : DependencyInfo
        {
        };
        using edge_t = std::variant<RequireEdge, ConstraintEdge>;

        using graph_t = DiGraph<node_t, edge_t>;
        using node_id = graph_t::node_id;
        using conflict_map = std::unordered_map<node_id, vector_set<node_id>>;

        static auto from_solver(MSolver const& solver, MPool const& pool) -> ProblemsGraph;

        ProblemsGraph() = delete;
        ProblemsGraph(graph_t graph, conflict_map conflicts, node_id root_node);

        auto graph() const noexcept -> graph_t const&;
        auto conflicts() const noexcept -> conflict_map const&;
        auto root_node() const noexcept -> node_id;

    private:
        graph_t m_graph;
        conflict_map m_conflicts;
        node_id m_root_node;
    };
}

#endif  // MAMBA_PROBLEMS_GRAPH_HPP
