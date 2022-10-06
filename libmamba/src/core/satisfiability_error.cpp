// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <vector>
#include <algorithm>
#include <map>
#include <type_traits>

#include <solv/pool.h>

#include "mamba/core/output.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/pool.hpp"

namespace mamba
{

    DependencyInfo::DependencyInfo(const std::string& dep)
    {
        static std::regex const regexp("\\s*(\\w[\\w-]*)\\s*([^\\s]*)(?:\\s+([^\\s]+))?\\s*");
        std::smatch matches;
        bool const matched = std::regex_match(dep, matches, regexp);
        // First match is the whole regex match
        if (!matched || matches.size() != 4)
        {
            throw std::runtime_error("Invalid dependency info: " + dep);
        }
        m_name = matches.str(1);
        m_version_range = matches.str(2);
        m_build_range = matches.str(3);
    }

    std::string const& DependencyInfo::name() const
    {
        return m_name;
    }

    std::string const& DependencyInfo::version_range() const
    {
        return m_version_range;
    }

    std::string const& DependencyInfo::build_range() const
    {
        return m_build_range;
    }

    std::string DependencyInfo::str() const
    {
        auto out = std::string(m_name);
        out.reserve(m_name.size() + (m_version_range.empty() ? 0 : 1) + m_version_range.size()
                    + (m_build_range.empty() ? 0 : 1) + m_version_range.size());
        if (!m_version_range.empty())
        {
            out += ' ';
            out += m_version_range;
        }
        if (!m_build_range.empty())
        {
            out += ' ';
            out += m_build_range;
        }
        return out;
    }

    namespace
    {

        auto warn_unexpected_problem(MSolverProblem const& problem) -> void
        {
            // TODO: Once the new error message are not experimental, we should consider
            // reducing this level since it is not somethig the user has control over.
            LOG_WARNING << "Unexpected empty optionals for problem type "
                        << solver_ruleinfo_name(problem.type);
        }

        class ProblemsGraphCreator
        {
        public:
            using SolvId = Id;  // Unscoped from libsolv

            using graph_t = ProblemsGraph::graph_t;
            using RootNode = ProblemsGraph::RootNode;
            using PackageNode = ProblemsGraph::PackageNode;
            using UnresolvedDependencyNode = ProblemsGraph::UnresolvedDependencyNode;
            using ConstraintNode = ProblemsGraph::ConstraintNode;
            using node_t = ProblemsGraph::node_t;
            using node_id = ProblemsGraph::node_id;
            using edge_t = ProblemsGraph::edge_t;
            using conflict_map = ProblemsGraph::conflict_map;

            ProblemsGraphCreator(MSolver const& solver, MPool const& pool)
                : m_solver{ solver }
                , m_pool{ pool }
            {
                m_root_node = m_graph.add_node(RootNode());
                parse_problems();
            }

            operator ProblemsGraph() &&
            {
                return { std::move(m_graph), std::move(m_conflicts), m_root_node };
            }

        private:
            MSolver const& m_solver;
            MPool const& m_pool;
            graph_t m_graph;
            conflict_map m_conflicts;
            std::map<SolvId, node_id> m_solv2node;
            node_id m_root_node;

            /**
             * Add a node and return the node id.
             *
             * If the node is already present and ``update`` is false then the current
             * node is left as it is, otherwise the new value is inserted.
             */
            auto add_solvable(SolvId solv_id, node_t&& pkg_info, bool update = true) -> node_id;

            auto add_conflict(node_id n1, node_id n2) -> void;
            [[nodiscard]] auto add_expanded_deps_edges(node_id from_id,
                                                       SolvId dep_id,
                                                       edge_t const& edge) -> bool;

            auto parse_problems() -> void;
        };

        auto ProblemsGraphCreator::add_solvable(SolvId solv_id, node_t&& node, bool update)
            -> node_id
        {
            if (auto const iter = m_solv2node.find(solv_id); iter != m_solv2node.end())
            {
                auto const id = iter->second;
                if (update)
                {
                    m_graph.node(id) = std::move(node);
                }
                return id;
            }
            auto const id = m_graph.add_node(std::move(node));
            m_solv2node[solv_id] = id;
            return id;
        };

        auto ProblemsGraphCreator::add_conflict(node_id n1, node_id n2) -> void
        {
            m_conflicts[n1].insert(n2);
            m_conflicts[n2].insert(n1);
        }

        auto ProblemsGraphCreator::add_expanded_deps_edges(node_id from_id,
                                                           SolvId dep_id,
                                                           edge_t const& edge) -> bool
        {
            auto added = false;
            for (const auto& solv_id : m_pool.select_solvables(dep_id))
            {
                added = true;
                auto pkg_info = PackageInfo(pool_id2solvable(m_pool, solv_id));
                auto to_id = add_solvable(
                    solv_id, PackageNode{ std::move(pkg_info), std::nullopt }, false);
                m_graph.add_edge(from_id, to_id, edge);
            }
            return added;
        }

        auto ProblemsGraphCreator::parse_problems() -> void
        {
            for (const auto& problem : m_solver.all_problems_structured())
            {
                // These functions do not return a reference so we make sure to
                // compute the value only once.
                auto source = problem.source();
                auto target = problem.target();
                auto dep = problem.dep();
                auto const type = problem.type;

                switch (type)
                {
                    case SOLVER_RULE_PKG_CONSTRAINS:
                    {
                        // SOLVER_RULE_PKG_CONSTRAINS has a dep, but it can resolve to nothing.
                        // The constraint conflict is actually expressed between the target and
                        // a constrains node child of the source.
                        if (!source || !target || !dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id
                            = add_solvable(problem.source_id,
                                           PackageNode{ std::move(source).value(), std::nullopt });
                        auto tgt_id = add_solvable(
                            problem.target_id, PackageNode{ std::move(target).value(), { type } });
                        auto cons_id = add_solvable(problem.dep_id, ConstraintNode{ dep.value() });
                        auto edge = DependencyInfo(dep.value());
                        m_graph.add_edge(src_id, cons_id, std::move(edge));
                        add_conflict(cons_id, tgt_id);
                        break;
                    }
                    case SOLVER_RULE_PKG_REQUIRES:
                    {
                        if (!dep || !source)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id
                            = add_solvable(problem.source_id,
                                           PackageNode{ std::move(source).value(), std::nullopt });
                        auto edge = DependencyInfo(dep.value());
                        auto added = add_expanded_deps_edges(src_id, problem.dep_id, edge);
                        if (!added)
                        {
                            LOG_WARNING << "Added empty dependency for problem type "
                                        << solver_ruleinfo_name(type);
                        }
                        break;
                    }
                    case SOLVER_RULE_JOB:
                    case SOLVER_RULE_PKG:
                    {
                        if (!dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto edge = DependencyInfo(dep.value());
                        auto added = add_expanded_deps_edges(m_root_node, problem.dep_id, edge);
                        if (!added)
                        {
                            LOG_WARNING << "Added empty dependency for problem type "
                                        << solver_ruleinfo_name(type);
                        }
                        break;
                    }
                    case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
                    case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                    {
                        if (!dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto edge = DependencyInfo(dep.value());
                        auto tgt_id = add_solvable(problem.target_id,
                                                   PackageNode{ std::move(dep).value(), { type } });
                        m_graph.add_edge(m_root_node, tgt_id, std::move(edge));
                        break;
                    }
                    case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
                    {
                        if (!source || !dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto edge = DependencyInfo(dep.value());
                        auto src_id
                            = add_solvable(problem.source_id,
                                           PackageNode{ std::move(source).value(), std::nullopt });
                        auto tgt_id = add_solvable(
                            problem.target_id, UnresolvedDependencyNode{ std::move(dep).value() });
                        m_graph.add_edge(src_id, tgt_id, std::move(edge));
                        break;
                    }
                    case SOLVER_RULE_PKG_CONFLICTS:
                    case SOLVER_RULE_PKG_SAME_NAME:
                    {
                        if (!source || !target)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id = add_solvable(
                            problem.source_id, PackageNode{ std::move(source).value(), { type } });
                        auto tgt_id = add_solvable(
                            problem.target_id, PackageNode{ std::move(target).value(), { type } });
                        add_conflict(src_id, tgt_id);
                        break;
                    }
                    case SOLVER_RULE_UPDATE:
                    {
                        // Explicitly ignored until we do something with it.
                        break;
                    }
                    default:
                    {
                        LOG_WARNING << "Problem type not implemented "
                                    << solver_ruleinfo_name(type);
                        break;
                    }
                }
            };
        }
    }

    auto ProblemsGraph::from_solver(MSolver const& solver, MPool const& pool) -> ProblemsGraph
    {
        return ProblemsGraphCreator(solver, pool);
    }

    ProblemsGraph::ProblemsGraph(graph_t graph, conflict_map conflicts, node_id root_node)
        : m_graph(std::move(graph))
        , m_conflicts(std::move(conflicts))
        , m_root_node(root_node)
    {
    }

    auto ProblemsGraph::graph() const noexcept -> graph_t const&
    {
        return m_graph;
    }

    auto ProblemsGraph::conflicts() const noexcept -> conflict_map const&
    {
        return m_conflicts;
    }

    auto ProblemsGraph::root_node() const noexcept -> node_id
    {
        return m_root_node;
    }
}
