// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_GRAPH_CREATOR_HPP
#define MAMBA_PROBLEMS_GRAPH_CREATOR_HPP

#include <string>
#include <utility>
#include <vector>
#include <regex>
#include <variant>
#include <unordered_set>

#include "mamba/core/pool.hpp"
#include "mamba/core/problems_graph_util.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/solver.hpp"

extern "C"
{
#include "solv/solver.h"
}

namespace mamba
{
    class MProblemsGraphCreator
    {
    public:
        using graph_t = MProblemsGraph<MNode, MEdge>;
        using node_id = MProblemsGraph<MNode, MEdge>::node_id;
        using solv_id_to_node_id = std::unordered_map<Id, node_id>;

        MProblemsGraphCreator(MPool* pool);

        const graph_t& graph_from(const std::vector<MSolverProblem>& problems);

    private:
        MPool* m_pool;
        graph_t m_problems_graph;
        solv_id_to_node_id m_solv_id_to_node_id;
        node_id m_root_id;

        void add_to_graph(const MSolverProblem& problem);
        void add_root_edge(Id target_id, const MNode& target_node, const MEdge& edge);
        void add_edge(Id source_id,
                      const MNode& source_node,
                      Id target_id,
                      const MNode& target_node,
                      const MEdge& edge);
        void add_conflicts(Id id1, const MNode& node1, Id id2, const MNode& node2);
        node_id get_update_or_create(Id source_id, const MNode& node);
    };

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

    inline std::optional<ProblemType> from(SolverRuleinfo solverRuleInfo)
    {
        switch (solverRuleInfo)
        {
            case SOLVER_RULE_PKG_CONSTRAINS:
            case SOLVER_RULE_PKG_REQUIRES:
            case SOLVER_RULE_JOB:
            case SOLVER_RULE_PKG:
            case SOLVER_RULE_UPDATE:
                return std::nullopt;
                ;
            case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                return ProblemType::NOT_FOUND;
            case SOLVER_RULE_PKG_CONFLICTS:
            case SOLVER_RULE_PKG_SAME_NAME:
                return ProblemType::CONFLICT;
            case SOLVER_RULE_BEST:
                return ProblemType::BEST_NOT_INSTALLABLE;
            case SOLVER_RULE_BLACK:
                return ProblemType::ONLY_DIRECT_INSTALL;
            case SOLVER_RULE_INFARCH:
                return ProblemType::INFERIOR_ARCH;
            case SOLVER_RULE_PKG_NOT_INSTALLABLE:
                return ProblemType::NOT_INSTALLABLE;
            case SOLVER_RULE_STRICT_REPO_PRIORITY:
                return ProblemType::EXCLUDED_BY_REPO_PRIORITY;
            case SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM:
                return ProblemType::PROVIDED_BY_SYSTEM;
            case SOLVER_RULE_CHOICE:
            case SOLVER_RULE_FEATURE:
            case SOLVER_RULE_JOB_UNSUPPORTED:
            case SOLVER_RULE_LEARNT:
            case SOLVER_RULE_PKG_RECOMMENDS:
            case SOLVER_RULE_RECOMMENDS:
            case SOLVER_RULE_UNKNOWN:
                return std::nullopt;
            case SOLVER_RULE_PKG_SELF_CONFLICT:
            case SOLVER_RULE_PKG_OBSOLETES:
            case SOLVER_RULE_PKG_IMPLICIT_OBSOLETES:
            case SOLVER_RULE_PKG_INSTALLED_OBSOLETES:
            case SOLVER_RULE_YUMOBS:
            case SOLVER_RULE_DISTUPGRADE:
                return std::nullopt;
        }
        return std::nullopt;
    }
}

#endif  // MAMBA_PROBLEMS_GRAPH_CREATOR_HPP