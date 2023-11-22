// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <sstream>
#include <stdexcept>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include <solv/pool.h>
#include <solv/solvable.h>
#include <solv/solver.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/core/solver.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/solver.hpp"

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

    MSolver::operator const Solver*() const
    {
        return solver().raw();
    }

    MSolver::operator Solver*()
    {
        return solver().raw();
    }

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

    void MSolver::add_reinstall_job(MatchSpec& ms, int job_flag)
    {
        auto solvable = std::optional<solv::ObjSolvableViewConst>{};

        // the data about the channel is only in the prefix_data unfortunately
        m_pool.pool().for_each_installed_solvable(
            [&](solv::ObjSolvableViewConst s)
            {
                if (s.name() == ms.name)
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

        if (!ms.channel.empty() || !ms.version.empty() || !ms.build_string.empty())
        {
            Console::stream() << ms.conda_build_form()
                              << ": overriding channel, version and build from "
                                 "installed packages due to --force-reinstall.";
            ms.channel = "";
            ms.version = "";
            ms.build_string = "";
        }

        MatchSpec modified_spec(ms);
        {
            auto channels = m_pool.channel_context().make_channel(std::string(solvable->channel()));
            if (channels.size() == 1)
            {
                modified_spec.channel = channels.front().display_name();
            }
            else
            {
                // If there is more than one, it's a custom_multi_channel name.
                // This should never happen.
                modified_spec.channel = solvable->channel();
            }
        }

        modified_spec.version = solvable->version();
        modified_spec.build_string = solvable->build_string();

        LOG_INFO << "Reinstall " << modified_spec.conda_build_form() << " from channel "
                 << modified_spec.channel;
        // TODO Fragile! The only reason why this works is that with a channel specific matchspec
        // the job will always be reinstalled.
        m_jobs->push_back(job_flag | SOLVER_SOLVABLE_PROVIDES, m_pool.matchspec2id(modified_spec));
    }

    void MSolver::add_jobs(const std::vector<std::string>& jobs, int job_flag)
    {
        for (const auto& job : jobs)
        {
            MatchSpec ms{ job, m_pool.channel_context() };
            int job_type = job_flag & SOLVER_JOBMASK;

            if (ms.conda_build_form().empty())
            {
                return;
            }

            if (job_type & SOLVER_INSTALL)
            {
                m_install_specs.emplace_back(job, m_pool.channel_context());
            }
            else if (job_type == SOLVER_ERASE)
            {
                m_remove_specs.emplace_back(job, m_pool.channel_context());
            }
            else if (job_type == SOLVER_LOCK)
            {
                m_neuter_specs.emplace_back(job, m_pool.channel_context());  // not used for the
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

    void MSolver::add_constraint(const std::string& job)
    {
        m_jobs->push_back(
            SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
            m_pool.matchspec2id({ job, m_pool.channel_context() })
        );
    }

    void MSolver::add_pin(const std::string& pin)
    {
        // In libsolv, locking means that a package keeps the same state: if it is installed,
        // it remains installed, if not it remains uninstalled.
        // Locking on a spec applies the lock to all packages matching the spec.
        // In mamba, we do not want to lock the package because we want to allow other variants
        // (matching the same spec) to unlock more solutions.
        // For instance we may pin ``libfmt=8.*`` but allow it to be swaped with a version built
        // by a more recent compiler.
        //
        // A previous version of this function would use ``SOLVER_LOCK`` to lock all packages not
        // matching the pin.
        // That played poorly with ``all_problems_structured`` because we could not interpret
        // the ids that were returned (since they were not associated with a single reldep).
        //
        // Another wrong idea is to add the pin as an install job.
        // This is not what is expected of pins, as they must not be installed if they were not
        // in the environement.
        // They can be configure in ``.condarc`` for generally specifying what versions are wanted.
        //
        // The idea behind the current version is to add the pin/spec as a constraint that must be
        // fullfield only if the package is installed.
        // This is not supported on solver jobs but it is on ``Solvable`` with
        // ``disttype == DISTYPE_CONDA``.
        // Therefore, we add a dummy solvable marked as already installed, and add the pin/spec
        // as one of its constrains.
        // Then we lock this solvable and force the re-checking of its dependencies.

        const auto pin_ms = MatchSpec{ pin, m_pool.channel_context() };
        m_pinned_specs.push_back(pin_ms);

        auto& pool = m_pool.pool();
        if (pool.disttype() != DISTTYPE_CONDA)
        {
            throw std::runtime_error("Cannot add pin to a pool that is not of Conda distype");
        }
        auto installed = pool.installed_repo();
        if (!installed.has_value())
        {
            throw std::runtime_error("Cannot add pin without a repo of installed packages");
        }

        // Add dummy solvable with a constraint on the pin (not installed if not present)
        auto [cons_solv_id, cons_solv] = installed->add_solvable();
        // TODO set some "pin" key on the solvable so that we can retrieve it during error messages
        const std::string cons_solv_name = fmt::format("pin-{}", m_pinned_specs.size());
        cons_solv.set_name(cons_solv_name);
        cons_solv.set_version("1");
        cons_solv.add_constraints(solv::ObjQueue{ m_pool.matchspec2id(pin_ms) });

        // Solvable need to provide itself
        cons_solv.add_self_provide();

        // Even if we lock it, libsolv may still try to remove it with
        // `SOLVER_FLAG_ALLOW_UNINSTALL`, so we flag it as not a real package to filter it out in
        // the transaction
        cons_solv.set_artificial(true);

        // Necessary for attributes to be properly stored
        installed->internalize();

        // WARNING keep separate or libsolv does not understand
        // Force verify the dummy solvable dependencies, as this is not the default for
        // installed packages.
        add_jobs({ cons_solv_name }, SOLVER_VERIFY);
        // Lock the dummy solvable so that it stays install.
        add_jobs({ cons_solv_name }, SOLVER_LOCK);
    }

    void MSolver::add_pins(const std::vector<std::string>& pins)
    {
        for (auto pin : pins)
        {
            add_pin(pin);
        }
    }

    void MSolver::py_set_postsolve_flags(const std::vector<std::pair<int, int>>& flags)
    {
        for (const auto& option : flags)
        {
            switch (option.first)
            {
                case PY_MAMBA_NO_DEPS:
                    m_flags.keep_dependencies = !option.second;
                    break;
                case PY_MAMBA_ONLY_DEPS:
                    m_flags.keep_specs = !option.second;
                    break;
                case PY_MAMBA_FORCE_REINSTALL:
                    m_flags.force_reinstall = option.second;
                    break;
            }
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
            solver_set_flag(*this, option.first, option.second);
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

    const std::vector<MatchSpec>& MSolver::install_specs() const
    {
        return m_install_specs;
    }

    const std::vector<MatchSpec>& MSolver::remove_specs() const
    {
        return m_remove_specs;
    }

    const std::vector<MatchSpec>& MSolver::neuter_specs() const
    {
        return m_neuter_specs;
    }

    const std::vector<MatchSpec>& MSolver::pinned_specs() const
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
        MSolverProblem make_solver_problem(
            const MSolver& solver,
            const MPool& pool,
            SolverRuleinfo type,
            Id source_id,
            Id target_id,
            Id dep_id
        )
        {
            const ::Solver* const solver_ptr = solver;
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
                    const_cast<::Solver*>(solver_ptr),  // Not const because might alloctmp space
                    type,
                    source_id,
                    target_id,
                    dep_id
                ),
            };
        }
    }

    std::vector<MSolverProblem> MSolver::all_problems_structured() const
    {
        std::vector<MSolverProblem> res = {};
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

        void warn_unexpected_problem(const MSolverProblem& problem)
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
            auto& channel_context = m_pool.channel_context();
            for (auto& problem : m_solver.all_problems_structured())
            {
                std::optional<PackageInfo>& source = problem.source;
                std::optional<PackageInfo>& target = problem.target;
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
                            ConstraintNode{ { dep.value(), channel_context } }
                        );
                        MatchSpec edge(dep.value(), channel_context);
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
                        MatchSpec edge(dep.value(), channel_context);
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
                        MatchSpec edge(dep.value(), channel_context);
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
                        MatchSpec edge(dep.value(), channel_context);
                        node_id dep_id = add_solvable(
                            problem.dep_id,
                            UnresolvedDependencyNode{ { std::move(dep).value(), channel_context } }
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
                        MatchSpec edge(dep.value(), channel_context);
                        node_id src_id = add_solvable(
                            problem.source_id,
                            PackageNode{ std::move(source).value() }
                        );
                        node_id dep_id = add_solvable(
                            problem.dep_id,
                            UnresolvedDependencyNode{ { std::move(dep).value(), channel_context } }
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
                        MatchSpec edge(source.value().name, channel_context);
                        // The package cannot exist without its name in the pool
                        assert(m_pool.pool().find_string(edge.name).has_value());
                        const auto dep_id = m_pool.pool().find_string(edge.name).value();
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
