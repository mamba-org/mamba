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
#include "solv-cpp/queue.hpp"
#include "solv-cpp/solver.hpp"

#include "solver/libsolv/helpers.hpp"

namespace mamba
{
    MSolver::MSolver(MPool pool, std::vector<std::pair<int, int>> flags)
        : m_libsolv_flags(std::move(flags))
        , m_pool(std::move(pool))
        , m_solver(nullptr)
        , m_jobs(std::make_unique<solv::ObjQueue>())
        , m_is_solved(false)
    {
        // TODO should we lazyly create solver here? Should we what provides?
        m_pool.create_whatprovides();
    }

    MSolver::~MSolver() = default;

    MSolver::MSolver(MSolver&&) = default;

    MSolver& MSolver::operator=(MSolver&&) = default;

    auto MSolver::solver() -> solv::ObjSolver&
    {
        return *m_solver;
    }

    auto MSolver::solver() const -> const solv::ObjSolver&
    {
        return *m_solver;
    }

    void MSolver::add_global_job(int job_flag)
    {
        m_jobs->push_back(job_flag, 0);
    }

    void MSolver::add_reinstall_job(const specs::MatchSpec& ms, int job_flag)
    {
        auto solvable = std::optional<solv::ObjSolvableViewConst>{};

        // the data about the channel is only in the prefix_data unfortunately
        m_pool.pool().for_each_installed_solvable(
            [&](solv::ObjSolvableViewConst s)
            {
                if (ms.name().contains(s.name()))
                {
                    solvable = s;
                    return solv::LoopControl::Break;
                }
                return solv::LoopControl::Continue;
            }
        );

        if (!solvable.has_value() || solvable->channel().empty())
        {
            // We are not reinstalling but simply installing.
            // Right now, using `--force-reinstall` will send all specs (whether they have
            // been previously installed or not) down this path, so we need to handle specs
            // that are not installed.
            return m_jobs->push_back(job_flag | SOLVER_SOLVABLE_PROVIDES, m_pool.matchspec2id(ms));
        }

        if (ms.channel().has_value() || !ms.version().is_explicitly_free()
            || !ms.build_string().is_free())
        {
            Console::stream() << ms.conda_build_form()
                              << ": overriding channel, version and build from "
                                 "installed packages due to --force-reinstall.";
        }

        auto ms_modified = ms;
        ms_modified.set_channel(specs::UnresolvedChannel::parse(solvable->channel()));
        ms_modified.set_version(specs::VersionSpec::parse(solvable->version()));
        ms_modified.set_build_string(specs::GlobSpec(std::string(solvable->build_string())));

        LOG_INFO << "Reinstall " << ms_modified.conda_build_form() << " from channel "
                 << ms_modified.channel()->str();
        // TODO Fragile! The only reason why this works is that with a channel specific matchspec
        // the job will always be reinstalled.
        m_jobs->push_back(job_flag | SOLVER_SOLVABLE_PROVIDES, m_pool.matchspec2id(ms_modified));
    }

    void MSolver::add_request(const Request& request)
    {
        for (const auto& item : request.items)
        {
            std::visit([&](const auto& r) { add_job_impl(r); }, item);
        }
    }

    void MSolver::add_job_impl(const Request::Install& job)
    {
        m_install_specs.emplace_back(job.spec);
        if (m_flags.force_reinstall)
        {
            add_reinstall_job(job.spec, SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES);
        }
        else
        {
            const auto job_id = m_pool.matchspec2id(job.spec);
            m_jobs->push_back(SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, job_id);
        }
    }

    void MSolver::add_job_impl(const Request::Remove& job)
    {
        m_remove_specs.emplace_back(job.spec);
        const auto job_id = m_pool.matchspec2id(job.spec);
        m_jobs->push_back(
            SOLVER_ERASE | SOLVER_SOLVABLE_PROVIDES
                | (job.clean_dependencies ? SOLVER_CLEANDEPS : SOLVER_ERASE),
            job_id
        );
    }

    void MSolver::add_job_impl(const Request::Update& job)
    {
        m_install_specs.emplace_back(job.spec);
        m_remove_specs.emplace_back(job.spec);
        const auto job_id = m_pool.matchspec2id(job.spec);
        // TODO: ignoring update specs here for now
        if (!job.spec.is_simple())
        {
            m_jobs->push_back(SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, job_id);
        }
        m_jobs->push_back(SOLVER_UPDATE | SOLVER_SOLVABLE_PROVIDES, job_id);
    }

    void MSolver::add_job_impl(const Request::Freeze& job)
    {
        m_neuter_specs.emplace_back(job.spec);
        const auto job_id = m_pool.matchspec2id(job.spec);
        m_jobs->push_back(SOLVER_LOCK, job_id);
    }

    void MSolver::add_job_impl(const Request::Keep& job)
    {
        const auto job_id = m_pool.matchspec2id(job.spec);
        m_jobs->push_back(SOLVER_USERINSTALLED, job_id);
    }

    void MSolver::add_job_impl(const Request::Pin& job)
    {
        add_pin(job.spec);
    }

    void MSolver::add_jobs(const std::vector<std::string>& jobs, int job_flag)
    {
        for (const auto& job : jobs)
        {
            auto ms = specs::MatchSpec::parse(job);
            int job_type = job_flag & SOLVER_JOBMASK;

            if (ms.conda_build_form().empty())
            {
                return;
            }

            if (job_type & SOLVER_INSTALL)
            {
                m_install_specs.emplace_back(specs::MatchSpec::parse(job));
            }
            else if (job_type == SOLVER_ERASE)
            {
                m_remove_specs.emplace_back(specs::MatchSpec::parse(job));
            }
            else if (job_type == SOLVER_LOCK)
            {
                m_neuter_specs.emplace_back(specs::MatchSpec::parse(job));  // not used for the
                                                                            // moment
            }

            const ::Id job_id = m_pool.matchspec2id(ms);

            // This is checking if SOLVER_ERASE and SOLVER_INSTALL are set
            // which are the flags for SOLVER_UPDATE
            if ((job_flag & SOLVER_UPDATE) == SOLVER_UPDATE)
            {
                // ignoring update specs here for now
                if (!ms.is_simple())
                {
                    m_jobs->push_back(SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, job_id);
                }
                m_jobs->push_back(job_flag | SOLVER_SOLVABLE_PROVIDES, job_id);
            }
            else if ((job_flag & SOLVER_INSTALL) && m_flags.force_reinstall)
            {
                add_reinstall_job(ms, job_flag);
            }
            else
            {
                LOG_INFO << "Adding job: " << ms.str();
                m_jobs->push_back(job_flag | SOLVER_SOLVABLE_PROVIDES, job_id);
            }
        }
    }

    void MSolver::add_pin(const specs::MatchSpec& pin)
    {
        solver::libsolv::pool_add_pin(m_pool.pool(), pin, m_pool.channel_context().params())
            .transform(
                [&](solv::ObjSolvableView pin_solv)
                {
                    m_pinned_specs.push_back(std::move(pin));

                    // WARNING keep separate or libsolv does not understand
                    // Force verify the dummy solvable dependencies, as this is not the default for
                    // installed packages.
                    add_jobs({ std::string(pin_solv.name()) }, SOLVER_VERIFY);
                    // Lock the dummy solvable so that it stays install.
                    add_jobs({ std::string(pin_solv.name()) }, SOLVER_LOCK);
                }
            )
            .or_else([](mamba_error&& error) { throw std::move(error); });
    }

    void MSolver::add_pins(const std::vector<std::string>& pins)
    {
        for (auto pin : pins)
        {
            add_pin(specs::MatchSpec::parse(pin));
        }
    }

    void MSolver::set_flags(const Flags& flags)
    {
        m_flags = flags;
    }

    auto MSolver::flags() const -> const Flags&
    {
        return m_flags;
    }

    void MSolver::py_set_libsolv_flags(const std::vector<std::pair<int, int>>& flags)
    {
        m_libsolv_flags = flags;
    }

    void MSolver::apply_libsolv_flags()
    {
        // TODO use new API
        for (const auto& option : m_libsolv_flags)
        {
            solver_set_flag(m_solver->raw(), option.first, option.second);
        }
    }

    bool MSolver::is_solved() const
    {
        return m_is_solved;
    }

    const MPool& MSolver::pool() const&
    {
        return m_pool;
    }

    MPool& MSolver::pool() &
    {
        return m_pool;
    }

    MPool&& MSolver::pool() &&
    {
        return std::move(m_pool);
    }

    const std::vector<specs::MatchSpec>& MSolver::install_specs() const
    {
        return m_install_specs;
    }

    const std::vector<specs::MatchSpec>& MSolver::remove_specs() const
    {
        return m_remove_specs;
    }

    const std::vector<specs::MatchSpec>& MSolver::neuter_specs() const
    {
        return m_neuter_specs;
    }

    const std::vector<specs::MatchSpec>& MSolver::pinned_specs() const
    {
        return m_pinned_specs;
    }

    bool MSolver::try_solve()
    {
        m_solver = std::make_unique<solv::ObjSolver>(m_pool.pool());
        apply_libsolv_flags();

        const bool success = solver().solve(m_pool.pool(), *m_jobs);
        m_is_solved = true;
        LOG_INFO << "Problem count: " << solver().problem_count();
        Console::instance().json_write({ { "success", success } });
        return success;
    }

    void MSolver::must_solve()
    {
        const bool success = try_solve();
        if (!success)
        {
            explain_problems(LOG_ERROR);
            throw mamba_error(
                "Could not solve for environment specs",
                mamba_error_code::satisfiablitity_error
            );
        }
    }

    namespace
    {
        // TODO change MSolver problem
        SolverProblem make_solver_problem(
            const MSolver& solver,
            const MPool& pool,
            SolverRuleinfo type,
            Id source_id,
            Id target_id,
            Id dep_id
        )
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
                solver_problemruleinfo2str(
                    const_cast<::Solver*>(solver.solver().raw()),  // Not const because might
                                                                   // alloctmp space
                    type,
                    source_id,
                    target_id,
                    dep_id
                ),
            };
        }
    }

    std::vector<SolverProblem> MSolver::all_problems_structured() const
    {
        std::vector<SolverProblem> res = {};
        res.reserve(solver().problem_count());  // Lower bound
        solver().for_each_problem_id(
            [&](solv::ProblemId pb)
            {
                for (solv::RuleId const rule : solver().problem_rules(pb))
                {
                    auto info = solver().get_rule_info(m_pool.pool(), rule);
                    res.push_back(make_solver_problem(
                        /* solver= */ *this,
                        /* pool= */ m_pool,
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

    std::string MSolver::all_problems_to_str() const
    {
        std::stringstream problems;
        solver().for_each_problem_id(
            [&](solv::ProblemId pb)
            {
                for (solv::RuleId const rule : solver().problem_rules(pb))
                {
                    auto const info = solver().get_rule_info(m_pool.pool(), rule);
                    problems << "  - " << solver().rule_info_to_string(m_pool.pool(), info) << "\n";
                }
            }
        );
        return problems.str();
    }

    std::ostream& MSolver::explain_problems(std::ostream& out) const
    {
        const auto& ctx = m_pool.context();
        out << "Could not solve for environment specs\n";
        const auto pbs = problems_graph();
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

    std::string MSolver::explain_problems() const
    {
        std::stringstream ss;
        explain_problems(ss);
        return ss.str();
    }

    std::string MSolver::problems_to_str() const
    {
        std::stringstream problems;
        solver().for_each_problem_id(
            [&](solv::ProblemId pb)
            { problems << "  - " << solver().problem_to_string(m_pool.pool(), pb) << "\n"; }
        );
        return "Encountered problems while solving:\n" + problems.str();
    }

    std::vector<std::string> MSolver::all_problems() const
    {
        std::vector<std::string> problems;
        solver().for_each_problem_id(
            [&](solv::ProblemId pb)
            { problems.emplace_back(solver().problem_to_string(m_pool.pool(), pb)); }
        );
        return problems;
    }

    namespace
    {

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

            ProblemsGraphCreator(const MSolver& solver, const MPool& pool);

            ProblemsGraph problem_graph() &&;

        private:

            const MSolver& m_solver;
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

        ProblemsGraphCreator::ProblemsGraphCreator(const MSolver& solver, const MPool& pool)
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
            for (auto& problem : m_solver.all_problems_structured())
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

    auto MSolver::problems_graph() const -> ProblemsGraph
    {
        return ProblemsGraphCreator(*this, m_pool).problem_graph();
    }

}  // namespace mamba
