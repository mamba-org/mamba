// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <sstream>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <solv/pool.h>
#include <solv/solvable.h>
#include <solv/solver.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/solver.hpp"

#include "solver/libsolv/helpers.hpp"

namespace mamba
{
    namespace
    {
        void set_solver_flags(solv::ObjSolver& solver, const solver::Request::Flags& flags)
        {
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_ALLOW_DOWNGRADE, flags.allow_downgrade);
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_ALLOW_UNINSTALL, flags.allow_uninstall);
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_STRICT_REPO_PRIORITY, flags.strict_repo_priority);
        }
    }

    auto MSolver::solve(MPool& mpool, const Request& request) -> expected_t<Outcome>
    {
        auto& pool = mpool.pool();
        const auto& chan_params = mpool.channel_context().params();
        const auto& flags = request.flags;

        return solver::libsolv::request_to_decision_queue(request, pool, chan_params, flags.force_reinstall)
            .transform(
                [&](auto&& jobs) -> Outcome
                {
                    auto solver = std::make_unique<solv::ObjSolver>(pool);
                    set_solver_flags(*solver, flags);
                    const bool success = solver->solve(pool, jobs);
                    if (!success)
                    {
                        return { UnSolvable(std::move(solver)) };
                    }

                    auto trans = solv::ObjTransaction::from_solver(pool, *solver);
                    trans.order(pool);

                    if (!flags.keep_user_specs && flags.keep_dependencies)
                    {
                        return {
                            solver::libsolv::transaction_to_solution_only_deps(pool, trans, request)
                        };
                    }
                    else if (flags.keep_user_specs && !flags.keep_dependencies)
                    {
                        return {
                            solver::libsolv::transaction_to_solution_no_deps(pool, trans, request)
                        };
                    }
                    return { solver::libsolv::transaction_to_solution(pool, trans) };
                }
            );
    }

    UnSolvable::UnSolvable(std::unique_ptr<solv::ObjSolver>&& solver)
        : m_solver(std::move(solver))
    {
    }

    UnSolvable::UnSolvable(UnSolvable&&) = default;

    UnSolvable::~UnSolvable() = default;

    auto UnSolvable::operator=(UnSolvable&&) -> UnSolvable& = default;

    auto UnSolvable::solver() const -> const solv::ObjSolver&
    {
        assert(m_solver != nullptr);
        return *m_solver;
    }

    auto UnSolvable::all_problems_to_str(MPool& pool) const -> std::string
    {
        std::stringstream problems;
        solver().for_each_problem_id(
            [&](solv::ProblemId pb)
            {
                for (solv::RuleId const rule : solver().problem_rules(pb))
                {
                    auto const info = solver().get_rule_info(pool.pool(), rule);
                    problems << "  - " << solver().rule_info_to_string(pool.pool(), info) << "\n";
                }
            }
        );
        return problems.str();
    }

    auto UnSolvable::explain_problems_to(MPool& pool, std::ostream& out) const -> std::ostream&
    {
        const auto& ctx = pool.context();
        out << "Could not solve for environment specs\n";
        const auto pbs = problems_graph(pool);
        const auto pbs_simplified = simplify_conflicts(pbs);
        const auto cp_pbs = CompressedProblemsGraph::from_problems_graph(pbs_simplified);
        print_problem_tree_msg(
            out,
            cp_pbs,
            { /* .unavailable= */ ctx.graphics_params.palette.failure,
              /* .available= */ ctx.graphics_params.palette.success }
        );
        return out;
    }

    auto UnSolvable::explain_problems(MPool& pool) const -> std::string
    {
        std::stringstream ss;
        explain_problems_to(pool, ss);
        return ss.str();
    }

    auto UnSolvable::problems_to_str(MPool& pool) const -> std::string
    {
        std::stringstream problems;
        solver().for_each_problem_id(
            [&](solv::ProblemId pb)
            { problems << "  - " << solver().problem_to_string(pool.pool(), pb) << "\n"; }
        );
        return "Encountered problems while solving:\n" + problems.str();
    }

    auto UnSolvable::all_problems(MPool& pool) const -> std::vector<std::string>
    {
        std::vector<std::string> problems;
        solver().for_each_problem_id(
            [&](solv::ProblemId pb)
            { problems.emplace_back(solver().problem_to_string(pool.pool(), pb)); }
        );
        return problems;
    }

    namespace
    {
        struct SolverProblem
        {
            SolverRuleinfo type;
            Id source_id;
            Id target_id;
            Id dep_id;
            std::optional<specs::PackageInfo> source;
            std::optional<specs::PackageInfo> target;
            std::optional<std::string> dep;
            std::string description;
        };

        // TODO change MSolver problem
        auto make_solver_problem(
            const solv::ObjSolver& solver,
            const MPool& pool,
            SolverRuleinfo type,
            Id source_id,
            Id target_id,
            Id dep_id
        ) -> SolverProblem
        {
            return {
                /* .type= */ type,
                /* .source_id= */ source_id,
                /* .target_id= */ target_id,
                /* .dep_id= */ dep_id,
                /* .source= */ pool.id2pkginfo(source_id),
                /* .target= */ pool.id2pkginfo(target_id),
                /* .dep= */ pool.dep2str(dep_id),
                /* .description= */
                ::solver_problemruleinfo2str(
                    const_cast<::Solver*>(solver.raw()),  // Not const because might alloctmp space
                    type,
                    source_id,
                    target_id,
                    dep_id
                ),
            };
        }

        auto all_problems_structured(const MPool& pool, const solv::ObjSolver& solver)
            -> std::vector<SolverProblem>
        {
            std::vector<SolverProblem> res = {};
            res.reserve(solver.problem_count());  // Lower bound
            solver.for_each_problem_id(
                [&](solv::ProblemId pb)
                {
                    for (solv::RuleId const rule : solver.problem_rules(pb))
                    {
                        auto info = solver.get_rule_info(pool.pool(), rule);
                        res.push_back(make_solver_problem(
                            /* solver= */ solver,
                            /* pool= */ pool,
                            /* type= */ info.type,
                            /* source_id= */ info.from_id.value_or(0),
                            /* target_id= */ info.to_id.value_or(0),
                            /* dep_id= */ info.dep_id.value_or(0)
                        ));
                    }
                }
            );
            return res;
        }

        void warn_unexpected_problem(const SolverProblem& problem)
        {
            // TODO: Once the new error message are not experimental, we should consider
            // reducing this level since it is not somethig the user has control over.
            LOG_WARNING << "Unexpected empty optionals for problem type "
                        << solv::enum_name(problem.type);
        }

        class ProblemsGraphCreator
        {
        public:

            using graph_t = ProblemsGraph::graph_t;
            using RootNode = ProblemsGraph::RootNode;
            using PackageNode = ProblemsGraph::PackageNode;
            using UnresolvedDependencyNode = ProblemsGraph::UnresolvedDependencyNode;
            using ConstraintNode = ProblemsGraph::ConstraintNode;
            using node_t = ProblemsGraph::node_t;
            using node_id = ProblemsGraph::node_id;
            using edge_t = ProblemsGraph::edge_t;
            using conflicts_t = ProblemsGraph::conflicts_t;

            ProblemsGraphCreator(const MPool& pool, const solv::ObjSolver& solver);

            ProblemsGraph problem_graph() &&;

        private:

            const solv::ObjSolver& m_solver;
            const MPool& m_pool;
            graph_t m_graph;
            conflicts_t m_conflicts;
            std::map<solv::SolvableId, node_id> m_solv2node;
            node_id m_root_node;

            /**
             * Add a node and return the node id.
             *
             * If the node is already present and ``update`` is false then the current
             * node is left as it is, otherwise the new value is inserted.
             */
            node_id add_solvable(solv::SolvableId solv_id, node_t&& pkg_info, bool update = true);

            void add_conflict(node_id n1, node_id n2);
            [[nodiscard]] bool
            add_expanded_deps_edges(node_id from_id, solv::SolvableId dep_id, const edge_t& edge);

            void parse_problems();
        };

        ProblemsGraphCreator::ProblemsGraphCreator(const MPool& pool, const solv::ObjSolver& solver)
            : m_solver{ solver }
            , m_pool{ pool }
        {
            m_root_node = m_graph.add_node(RootNode());
            parse_problems();
        }

        ProblemsGraph ProblemsGraphCreator::problem_graph() &&
        {
            return { std::move(m_graph), std::move(m_conflicts), m_root_node };
        }

        auto ProblemsGraphCreator::add_solvable(solv::SolvableId solv_id, node_t&& node, bool update)
            -> node_id
        {
            if (const auto iter = m_solv2node.find(solv_id); iter != m_solv2node.end())
            {
                const node_id id = iter->second;
                if (update)
                {
                    m_graph.node(id) = std::move(node);
                }
                return id;
            }
            const node_id id = m_graph.add_node(std::move(node));
            m_solv2node[solv_id] = id;
            return id;
        };

        void ProblemsGraphCreator::add_conflict(node_id n1, node_id n2)
        {
            m_conflicts.add(n1, n2);
        }

        bool ProblemsGraphCreator::add_expanded_deps_edges(
            node_id from_id,
            solv::SolvableId dep_id,
            const edge_t& edge
        )
        {
            bool added = false;
            for (const auto& solv_id : m_pool.select_solvables(dep_id))
            {
                added = true;
                auto pkg_info = m_pool.id2pkginfo(solv_id);
                assert(pkg_info.has_value());
                node_id to_id = add_solvable(solv_id, PackageNode{ std::move(pkg_info).value() }, false);
                m_graph.add_edge(from_id, to_id, edge);
            }
            return added;
        }

        void ProblemsGraphCreator::parse_problems()
        {
            for (auto& problem : all_problems_structured(m_pool, m_solver))
            {
                std::optional<specs::PackageInfo>& source = problem.source;
                std::optional<specs::PackageInfo>& target = problem.target;
                std::optional<std::string>& dep = problem.dep;
                SolverRuleinfo type = problem.type;

                switch (type)
                {
                    case SOLVER_RULE_PKG_CONSTRAINS:
                    {
                        // A constraint (run_constrained) on source is conflicting with target.
                        // SOLVER_RULE_PKG_CONSTRAINS has a dep, but it can resolve to nothing.
                        // The constraint conflict is actually expressed between the target and
                        // a constrains node child of the source.
                        if (!source || !target || !dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id = add_solvable(
                            problem.source_id,
                            PackageNode{ std::move(source).value() }
                        );
                        node_id tgt_id = add_solvable(
                            problem.target_id,
                            PackageNode{ std::move(target).value() }
                        );
                        node_id cons_id = add_solvable(
                            problem.dep_id,
                            ConstraintNode{ specs::MatchSpec::parse(dep.value()) }
                        );
                        auto edge = specs::MatchSpec::parse(dep.value());
                        m_graph.add_edge(src_id, cons_id, std::move(edge));
                        add_conflict(cons_id, tgt_id);
                        break;
                    }
                    case SOLVER_RULE_PKG_REQUIRES:
                    {
                        // Express a dependency on source that is involved in explaining the
                        // problem.
                        // Not all dependency of package will appear, only enough to explain the
                        // problem. It is not a problem in itself, only a part of the graph.
                        if (!dep || !source)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto src_id = add_solvable(
                            problem.source_id,
                            PackageNode{ std::move(source).value() }
                        );
                        auto edge = specs::MatchSpec::parse(dep.value());
                        bool added = add_expanded_deps_edges(src_id, problem.dep_id, edge);
                        if (!added)
                        {
                            LOG_WARNING << "Added empty dependency for problem type "
                                        << solv::enum_name(type);
                        }
                        break;
                    }
                    case SOLVER_RULE_JOB:
                    case SOLVER_RULE_PKG:
                    {
                        // A top level requirement.
                        // The difference between JOB and PKG is unknown (possibly unused).
                        if (!dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto edge = specs::MatchSpec::parse(dep.value());
                        bool added = add_expanded_deps_edges(m_root_node, problem.dep_id, edge);
                        if (!added)
                        {
                            LOG_WARNING << "Added empty dependency for problem type "
                                        << solv::enum_name(type);
                        }
                        break;
                    }
                    case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
                    case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                    {
                        // A top level dependency does not exist.
                        // Could be a wrong name or missing channel.
                        if (!dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto edge = specs::MatchSpec::parse(dep.value());
                        node_id dep_id = add_solvable(
                            problem.dep_id,
                            UnresolvedDependencyNode{ specs::MatchSpec::parse(dep.value()) }
                        );
                        m_graph.add_edge(m_root_node, dep_id, std::move(edge));
                        break;
                    }
                    case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
                    {
                        // A package dependency does not exist.
                        // Could be a wrong name or missing channel.
                        // This is a partial exaplanation of why a specific solvable (could be any
                        // of the parent) cannot be installed.
                        if (!source || !dep)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        auto edge = specs::MatchSpec::parse(dep.value());
                        node_id src_id = add_solvable(
                            problem.source_id,
                            PackageNode{ std::move(source).value() }
                        );
                        node_id dep_id = add_solvable(
                            problem.dep_id,
                            UnresolvedDependencyNode{ specs::MatchSpec::parse(dep.value()) }
                        );
                        m_graph.add_edge(src_id, dep_id, std::move(edge));
                        break;
                    }
                    case SOLVER_RULE_PKG_CONFLICTS:
                    case SOLVER_RULE_PKG_SAME_NAME:
                    {
                        // Looking for a valid solution to the installation satisfiability expand to
                        // two solvables of same package that cannot be installed together. This is
                        // a partial exaplanation of why one of the solvables (could be any of the
                        // parent) cannot be installed.
                        if (!source || !target)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }
                        node_id src_id = add_solvable(
                            problem.source_id,
                            PackageNode{ std::move(source).value() }
                        );
                        node_id tgt_id = add_solvable(
                            problem.target_id,
                            PackageNode{ std::move(target).value() }
                        );
                        add_conflict(src_id, tgt_id);
                        break;
                    }
                    case SOLVER_RULE_UPDATE:
                    {
                        // Case where source is an installed package appearing in the problem.
                        // Contrary to its name upgrading it may not solve the problem (otherwise
                        // the solver would likely have done it).
                        if (!source)
                        {
                            warn_unexpected_problem(problem);
                            break;
                        }

                        // We re-create an dependency. There is no dependency ready to use for
                        // how the solver is handling this package, as this is resolved in term of
                        // installed packages and solver flags (allow downgrade...) rather than a
                        // dependency.
                        auto edge = specs::MatchSpec::parse(source.value().name);
                        // The package cannot exist without its name in the pool
                        assert(m_pool.pool().find_string(edge.name().str()).has_value());
                        const auto dep_id = m_pool.pool().find_string(edge.name().str()).value();
                        const bool added = add_expanded_deps_edges(m_root_node, dep_id, edge);
                        if (!added)
                        {
                            LOG_WARNING << "Added empty dependency for problem type "
                                        << solv::enum_name(type);
                        }
                        break;
                    }
                    default:
                    {
                        // Many more SolverRuleinfo that heve not been encountered.
                        LOG_WARNING << "Problem type not implemented " << solv::enum_name(type);
                        break;
                    }
                }
            }
        }
    }

    auto UnSolvable::problems_graph(const MPool& pool) const -> ProblemsGraph
    {
        assert(m_solver != nullptr);
        return ProblemsGraphCreator(pool, *m_solver).problem_graph();
    }

}  // namespace mamba
