// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <vector>
#include <algorithm>
#include <map>

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
        auto pkg_info_to_str(std::optional<PackageInfo> const& pkg_info) -> std::string
        {
            if (pkg_info)
            {
                return pkg_info.value().str();
            }
            return "(empty)";
        }

        auto solver_rule_name(SolverRuleinfo type)
        {
            switch (type)
            {
                case SOLVER_RULE_UNKNOWN:
                    return "SOLVER_RULE_UNKNOWN";
                case SOLVER_RULE_PKG:
                    return "SOLVER_RULE_PKG";
                case SOLVER_RULE_PKG_NOT_INSTALLABLE:
                    return "SOLVER_RULE_PKG_NOT_INSTALLABLE";
                case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
                    return "SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP";
                case SOLVER_RULE_PKG_REQUIRES:
                    return "SOLVER_RULE_PKG_REQUIRES";
                case SOLVER_RULE_PKG_SELF_CONFLICT:
                    return "SOLVER_RULE_PKG_SELF_CONFLICT";
                case SOLVER_RULE_PKG_CONFLICTS:
                    return "SOLVER_RULE_PKG_CONFLICTS";
                case SOLVER_RULE_PKG_SAME_NAME:
                    return "SOLVER_RULE_PKG_SAME_NAME";
                case SOLVER_RULE_PKG_OBSOLETES:
                    return "SOLVER_RULE_PKG_OBSOLETES";
                case SOLVER_RULE_PKG_IMPLICIT_OBSOLETES:
                    return "SOLVER_RULE_PKG_IMPLICIT_OBSOLETES";
                case SOLVER_RULE_PKG_INSTALLED_OBSOLETES:
                    return "SOLVER_RULE_PKG_INSTALLED_OBSOLETES";
                case SOLVER_RULE_PKG_RECOMMENDS:
                    return "SOLVER_RULE_PKG_RECOMMENDS";
                case SOLVER_RULE_PKG_CONSTRAINS:
                    return "SOLVER_RULE_PKG_CONSTRAINS";
                case SOLVER_RULE_UPDATE:
                    return "SOLVER_RULE_UPDATE";
                case SOLVER_RULE_FEATURE:
                    return "SOLVER_RULE_FEATURE";
                case SOLVER_RULE_JOB:
                    return "SOLVER_RULE_JOB";
                case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
                    return "SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP";
                case SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM:
                    return "SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM";
                case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                    return "SOLVER_RULE_JOB_UNKNOWN_PACKAGE";
                case SOLVER_RULE_JOB_UNSUPPORTED:
                    return "SOLVER_RULE_JOB_UNSUPPORTED";
                case SOLVER_RULE_DISTUPGRADE:
                    return "SOLVER_RULE_DISTUPGRADE";
                case SOLVER_RULE_INFARCH:
                    return "SOLVER_RULE_INFARCH";
                case SOLVER_RULE_CHOICE:
                    return "SOLVER_RULE_CHOICE";
                case SOLVER_RULE_LEARNT:
                    return "SOLVER_RULE_LEARNT";
                case SOLVER_RULE_BEST:
                    return "SOLVER_RULE_BEST";
                case SOLVER_RULE_YUMOBS:
                    return "SOLVER_RULE_YUMOBS";
                case SOLVER_RULE_RECOMMENDS:
                    return "SOLVER_RULE_RECOMMENDS";
                case SOLVER_RULE_BLACK:
                    return "SOLVER_RULE_BLACK";
                case SOLVER_RULE_STRICT_REPO_PRIORITY:
                    return "SOLVER_RULE_STRICT_REPO_PRIORITY";
                default:
                {
                    assert(false);
                    return "[SolverRule name not implemented]";
                }
            }
        }

        auto warn_unexpected_problem(MSolverProblem const& problem) -> void
        {
            // TODO source is recomputed.
            // TODO: Once the new error message are not experimental, we should consider
            // reducing this level since it is not somethig the user has control over.
            LOG_WARNING << "Unexpected empty optionals for problem type "
                        << solver_rule_name(problem.type)
                        << "\nSource: " << pkg_info_to_str(problem.source())
                        << "\nTarget: " << pkg_info_to_str(problem.target())
                        << "\nDep: " << problem.dep().value_or("(empty)");
        }

        std::optional<ProblemsGraph::ProblemType> map_problem(SolverRuleinfo solverRuleInfo)
        {
            switch (solverRuleInfo)
            {
                case SOLVER_RULE_PKG_CONSTRAINS:
                case SOLVER_RULE_PKG_REQUIRES:
                case SOLVER_RULE_JOB:
                case SOLVER_RULE_PKG:
                case SOLVER_RULE_UPDATE:
                    return std::nullopt;
                case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
                case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
                case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                    return ProblemsGraph::ProblemType::NOT_FOUND;
                case SOLVER_RULE_PKG_CONFLICTS:
                case SOLVER_RULE_PKG_SAME_NAME:
                    return ProblemsGraph::ProblemType::CONFLICT;
                case SOLVER_RULE_BEST:
                    return ProblemsGraph::ProblemType::BEST_NOT_INSTALLABLE;
                case SOLVER_RULE_BLACK:
                    return ProblemsGraph::ProblemType::ONLY_DIRECT_INSTALL;
                case SOLVER_RULE_INFARCH:
                    return ProblemsGraph::ProblemType::INFERIOR_ARCH;
                case SOLVER_RULE_PKG_NOT_INSTALLABLE:
                    return ProblemsGraph::ProblemType::NOT_INSTALLABLE;
                case SOLVER_RULE_STRICT_REPO_PRIORITY:
                    return ProblemsGraph::ProblemType::EXCLUDED_BY_REPO_PRIORITY;
                case SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM:
                    return ProblemsGraph::ProblemType::PROVIDED_BY_SYSTEM;
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

        class ProblemsGraphCreator
        {
        public:
            using SolvId = Id;  // Unscoped from libsolv

            using graph_t = ProblemsGraph::graph_t;
            using ProblemType = ProblemsGraph::ProblemType;
            using RootNode = ProblemsGraph::RootNode;
            using PackageNode = ProblemsGraph::PackageNode;
            using UnresolvedDependencyNode = ProblemsGraph::UnresolvedDependencyNode;
            using node_t = ProblemsGraph::node_t;
            using node_id = ProblemsGraph::node_id;
            using ConstraintEdge = ProblemsGraph::ConstraintEdge;
            using RequireEdge = ProblemsGraph::RequireEdge;
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
             * Add a node if it is not already present and return the node id.
             */
            auto ensure_solvable(SolvId solv_id, node_t&& pkg_info) -> node_id;
            auto ensure_solvable(SolvId solv_id,
                                 PackageInfo&& pkg_info,
                                 std::optional<ProblemType> pb_type) -> node_id;
            auto ensure_solvable(SolvId solv_id,
                                 std::string&& dep,
                                 std::optional<ProblemType> pb_type) -> node_id;

            auto add_conflict(node_id n1, node_id n2) -> void;
            auto add_expanded_deps_edges(node_id from_id, SolvId dep_id, edge_t const& edge)
                -> void;
            auto parse_problems() -> void;
        };

        auto ProblemsGraphCreator::ensure_solvable(SolvId solv_id, node_t&& node) -> node_id
        {
            if (auto const iter = m_solv2node.find(solv_id); iter != m_solv2node.end())
            {
                return iter->second;
            }
            auto const node_id = m_graph.add_node(std::move(node));
            m_solv2node[solv_id] = node_id;
            return node_id;
        };

        auto ProblemsGraphCreator::ensure_solvable(SolvId solv_id,
                                                   PackageInfo&& pkg_info,
                                                   std::optional<ProblemType> pb_type = {})
            -> node_id
        {
            return ensure_solvable(solv_id, PackageNode{ std::move(pkg_info), pb_type });
        }

        auto ProblemsGraphCreator::ensure_solvable(SolvId solv_id,
                                                   std::string&& dep,
                                                   std::optional<ProblemType> pb_type) -> node_id
        {
            return ensure_solvable(solv_id, UnresolvedDependencyNode{ std::move(dep), pb_type });
        }

        auto ProblemsGraphCreator::add_conflict(node_id n1, node_id n2) -> void
        {
            m_conflicts[n1].insert(n2);
            m_conflicts[n2].insert(n2);
        }

        auto ProblemsGraphCreator::add_expanded_deps_edges(node_id from_id,
                                                           SolvId dep_id,
                                                           edge_t const& edge) -> void
        {
            for (const auto& solv_id : m_pool.select_solvables(dep_id))
            {
                auto pkg_info = PackageInfo(pool_id2solvable(m_pool, solv_id));
                auto to_id = ensure_solvable(solv_id, std::move(pkg_info), {});
                m_graph.add_edge(from_id, to_id, edge);
            }
        }

        auto ProblemsGraphCreator::parse_problems() -> void
        {
            for (const auto& problem : m_solver.all_problems_structured())
            {
                // These functions do not return a reference so we make sure to
                // compute the value only once.
                // TODO change name of these functions to make explicit it is not a ref.
                auto source = problem.source();
                auto target = problem.target();
                auto dep = problem.dep();
                auto type = map_problem(problem.type);
                switch (problem.type)
                {
                    case SOLVER_RULE_PKG_CONSTRAINS:
                    {
                        // TODO
                        if (!source || !target || !dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id = ensure_solvable(problem.source_id, std::move(source).value());
                        auto tgt_id
                            = ensure_solvable(problem.target_id, std::move(target).value(), type);
                        auto const edge = ConstraintEdge{ DependencyInfo(dep.value()) };
                        add_expanded_deps_edges(src_id, problem.dep_id, edge);
                        add_conflict(src_id, tgt_id);
                        break;
                    }
                    case SOLVER_RULE_PKG_REQUIRES:
                    {
                        if (!dep || !source)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id = ensure_solvable(problem.source_id, std::move(source).value());
                        auto edge = RequireEdge{ DependencyInfo(dep.value()) };
                        add_expanded_deps_edges(src_id, problem.dep_id, edge);
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
                        // TODO check here that depId exists
                        auto edge = RequireEdge{ DependencyInfo(dep.value()) };
                        add_expanded_deps_edges(m_root_node, problem.dep_id, edge);
                        break;
                    }
                    case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
                    case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
                    case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                    {
                        if (!source || !dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto edge = RequireEdge{ DependencyInfo(dep.value()) };
                        auto src_id = ensure_solvable(problem.source_id, std::move(source).value());
                        auto tgt_id
                            = ensure_solvable(problem.target_id, std::move(dep).value(), type);
                        m_graph.add_edge(src_id, tgt_id, std::move(edge));
                    }
                    case SOLVER_RULE_PKG_CONFLICTS:
                    case SOLVER_RULE_PKG_SAME_NAME:
                    {
                        if (!source || !target)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id = ensure_solvable(problem.source_id, std::move(source).value());
                        auto tgt_id
                            = ensure_solvable(problem.target_id, std::move(target).value(), type);
                        add_conflict(src_id, tgt_id);
                        break;
                    }
                    default:
                    {
                        LOG_WARNING << "Problem type not implemented: " << problem.type;
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
