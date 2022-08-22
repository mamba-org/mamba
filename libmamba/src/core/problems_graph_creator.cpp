// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/problems_graph_creator.hpp"

namespace mamba
{
    MProblemsGraphCreator::MProblemsGraphCreator(MPool* pool)
        : m_pool(pool)
        , m_problems_graph()
    {
    }

    auto MProblemsGraphCreator::create_graph_from_problems(
        const std::vector<MSolverProblem>& problems, const std::vector<std::string>& initial_specs)
        -> const graph&
    {
        std::vector<std::string> initial_spec_names;
        for (const auto& initial_spec : initial_specs)
        {
            initial_spec_names.push_back(MatchSpec(initial_spec).name);
        }
        for (const auto& problem : problems)
        {
            add_problem_to_graph(problem, initial_spec_names);
        }

        return m_problems_graph;
    }

    void MProblemsGraphCreator::add_problem_to_graph(
        const MSolverProblem& problem, const std::vector<std::string>& initial_spec_names)
    {
        switch (problem.type)
        {
            case SOLVER_RULE_PKG_CONSTRAINS:
            {
                if (!has_values(problem, { problem.source(), problem.target() })
                    || !has_values(problem, { problem.dep() }))
                {
                    break;
                }
                auto source = MNode(problem.source().value());
                std::vector<Id> constrained_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& constraint_id : constrained_ids)
                {
                    PackageInfo constraint_pkg
                        = PackageInfo(pool_id2solvable(*m_pool, constraint_id));
                    m_problems_graph.add_edge(source, MNode(constraint_pkg), problem.dep().value());
                }
                m_problems_graph.add_conflicts(source, MNode(problem.target().value()));
                break;
            }
            case SOLVER_RULE_PKG_REQUIRES:
            {
                if (!has_values(problem, { problem.dep() })
                    || !has_values(problem, { problem.source() }))
                {
                    break;
                }
                std::vector<Id> target_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& target_id : target_ids)
                {
                    PackageInfo target = PackageInfo(pool_id2solvable(*m_pool, target_id));
                    m_problems_graph.add_edge(
                        MNode(problem.source().value()), MNode(target), problem.dep().value());
                }
                break;
            }
            case SOLVER_RULE_JOB:
            case SOLVER_RULE_PKG:
            {
                if (!has_values(problem, { problem.dep() }))
                {
                    break;
                }
                if (!contains_any_substring(problem.dep().value(), initial_spec_names))
                {
                    break;
                }
                std::vector<Id> target_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& target_id : target_ids)
                {
                    PackageInfo target = PackageInfo(pool_id2solvable(*m_pool, target_id));
                    m_problems_graph.add_edge(MNode(), MNode(target), problem.dep().value());
                }
                break;
            }
            case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
            {
                if (!has_values(problem, { problem.source() })
                    || !has_values(problem, { problem.dep() }))
                {
                    break;
                }
                m_problems_graph.add_edge(MNode(problem.source().value()),
                                          MNode(problem.dep().value(), problem.type),
                                          problem.dep().value());
                break;
            }
            case SOLVER_RULE_PKG_CONFLICTS:
            case SOLVER_RULE_PKG_SAME_NAME:
            {
                if (!has_values(problem, { problem.source(), problem.target() }))
                {
                    break;
                }
                m_problems_graph.add_conflicts(MNode(problem.source().value()),
                                               MNode(problem.target().value()));
                break;
            }
            case SOLVER_RULE_UPDATE:
            {
                if (!has_values(problem, { problem.source() }))
                {
                    break;
                }
                LOG_WARNING << "Update rules are unsupported for now" << std::endl;
                return;
            }
            case SOLVER_RULE_BEST:
            case SOLVER_RULE_BLACK:
            case SOLVER_RULE_DISTUPGRADE:
            case SOLVER_RULE_INFARCH:
            case SOLVER_RULE_PKG_NOT_INSTALLABLE:
            case SOLVER_RULE_STRICT_REPO_PRIORITY:
            {
                if (!has_values(problem, { problem.source() }))
                {
                    break;
                }
                m_problems_graph.add_node(MNode(problem.source().value(), problem.type));
                break;
            }
            case SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM:
            {
                LOG_WARNING << "Problem type not implemented: " << problem.type;
                break;
            }
            case SOLVER_RULE_CHOICE:
            case SOLVER_RULE_FEATURE:
            case SOLVER_RULE_JOB_UNSUPPORTED:
            case SOLVER_RULE_LEARNT:
            case SOLVER_RULE_PKG_RECOMMENDS:
            case SOLVER_RULE_RECOMMENDS:
            case SOLVER_RULE_UNKNOWN:
            {
                LOG_WARNING << "Problem type not implemented: " << problem.type;
                break;
            }
            case SOLVER_RULE_PKG_SELF_CONFLICT:
            case SOLVER_RULE_PKG_OBSOLETES:
            case SOLVER_RULE_PKG_IMPLICIT_OBSOLETES:
            case SOLVER_RULE_PKG_INSTALLED_OBSOLETES:
            case SOLVER_RULE_YUMOBS:
                LOG_WARNING << "Problem type not implemented: " << problem.type;
        }
    }
}