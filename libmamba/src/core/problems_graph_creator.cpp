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
        , m_solv_id_to_node_id()
    {
        auto root_node = MNode(NodeInfo::Root());
        m_root_id = m_problems_graph.graph().add_node(root_node);
    }

    auto MProblemsGraphCreator::graph_from(const std::vector<MSolverProblem>& problems)
        -> const graph_t&
    {
        for (const auto& problem : problems)
        {
            add_to_graph(problem);
        }

        return m_problems_graph;
    }

    void MProblemsGraphCreator::add_to_graph(const MSolverProblem& problem)
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
                auto source_node = MNode(NodeInfo::ResolvedPackage(problem.source().value()));
                auto edge = MEdge(EdgeInfo::Constraint(DependencyInfo(problem.dep().value())));
                std::vector<Id> constrained_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& constraint_id : constrained_ids)
                {
                    PackageInfo constraint_pkg
                        = PackageInfo(pool_id2solvable(*m_pool, constraint_id));
                    auto dest_node = MNode(NodeInfo::ResolvedPackage(constraint_pkg));
                    add_edge(problem.source_id, source_node, constraint_id, dest_node, edge);
                }
                auto target_node = MNode(NodeInfo::ResolvedPackage(problem.target().value()));
                add_conflicts(problem.source_id, source_node, problem.target_id, target_node);
                break;
            }
            case SOLVER_RULE_PKG_REQUIRES:
            {
                if (!has_values(problem, { problem.dep() })
                    || !has_values(problem, { problem.source() }))
                {
                    break;
                }
                auto source_node = MNode(NodeInfo::ResolvedPackage(problem.source().value()));
                auto edge = MEdge(EdgeInfo::Require(DependencyInfo(problem.dep().value())));
                std::vector<Id> required_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& required_id : required_ids)
                {
                    PackageInfo required_pkg = PackageInfo(pool_id2solvable(*m_pool, required_id));
                    auto dest_node = MNode(NodeInfo::ResolvedPackage(required_pkg));
                    add_edge(problem.source_id, source_node, required_id, dest_node, edge);
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
                // TODO check here that depId exists
                /*if (!contains_any_substring(problem.dep().value(), initial_spec_names))
                {
                    break;
                }*/
                auto edge = MEdge(EdgeInfo::Require(DependencyInfo(problem.dep().value())));
                std::vector<Id> target_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& target_id : target_ids)
                {
                    PackageInfo target_pkg = PackageInfo(pool_id2solvable(*m_pool, target_id));
                    auto target_node = MNode(NodeInfo::ResolvedPackage(target_pkg));
                    add_root_edge(target_id, target_node, edge);
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
                auto source_node = MNode(NodeInfo::ResolvedPackage(problem.source().value()));
                auto target_node = MNode(NodeInfo::ProblematicPackage(problem.dep().value()),
                                         from(problem.type));
                auto edge = MEdge(EdgeInfo::Require(DependencyInfo(problem.dep().value())));
                add_edge(problem.source_id, source_node, problem.target_id, target_node, edge);
                break;
            }
            case SOLVER_RULE_PKG_CONFLICTS:
            case SOLVER_RULE_PKG_SAME_NAME:
            {
                if (!has_values(problem, { problem.source(), problem.target() }))
                {
                    break;
                }
                auto source_node = MNode(NodeInfo::ResolvedPackage(problem.source().value()),
                                         from(problem.type));
                auto target_node = MNode(NodeInfo::ResolvedPackage(problem.target().value()),
                                         from(problem.type));
                add_conflicts(problem.source_id, source_node, problem.target_id, target_node);
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
            case SOLVER_RULE_INFARCH:
            case SOLVER_RULE_PKG_NOT_INSTALLABLE:
            case SOLVER_RULE_STRICT_REPO_PRIORITY:
            {
                if (!has_values(problem, { problem.source() }))
                {
                    break;
                }
                get_update_or_create(
                    problem.source_id,
                    MNode(NodeInfo::ResolvedPackage(problem.source().value()), from(problem.type)));
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
            case SOLVER_RULE_DISTUPGRADE:
                LOG_WARNING << "Problem type not implemented: " << problem.type;
        }
    }

    void MProblemsGraphCreator::add_edge(Id source_id,
                                         const MNode& source_node,
                                         Id target_id,
                                         const MNode& target_node,
                                         const MEdge& edge)
    {
        auto source_node_id = get_update_or_create(source_id, source_node);
        auto target_node_id = get_update_or_create(target_id, target_node);
        m_problems_graph.graph().add_edge(source_node_id, target_node_id, edge);
    }

    void MProblemsGraphCreator::add_root_edge(Id target_id,
                                              const MNode& target_node,
                                              const MEdge& edge)
    {
        auto target_node_id = get_update_or_create(target_id, target_node);
        m_problems_graph.graph().add_edge(m_root_id, target_node_id, edge);
    }

    void MProblemsGraphCreator::add_conflicts(Id id1,
                                              const MNode& node1,
                                              Id id2,
                                              const MNode& node2)
    {
        auto node_id1 = get_update_or_create(id1, node1);
        auto node_id2 = get_update_or_create(id2, node2);
        m_problems_graph.add_conflicts(node_id1, node_id2);
    }

    auto MProblemsGraphCreator::get_update_or_create(Id id, const MNode& node) -> node_id
    {
        auto it = m_solv_id_to_node_id.find(id);
        if (it != m_solv_id_to_node_id.end())
        {
            m_problems_graph.graph().nodes()[it->second].maybe_update_metadata(node);
            return it->second;
        }
        else
        {
            return m_solv_id_to_node_id[id] = m_problems_graph.graph().add_node(node);
        }
    }
}