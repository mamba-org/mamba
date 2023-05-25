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

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/core/solver.hpp"
#include "mamba/core/util_string.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/solver.hpp"

namespace mamba
{

    // TODO this should belong in libsolv.
    const char* solver_ruleinfo_name(SolverRuleinfo rule)
    {
        switch (rule)
        {
            case (SOLVER_RULE_UNKNOWN):
            {
                return "SOLVER_RULE_UNKNOWN";
            }
            case (SOLVER_RULE_PKG):
            {
                return "SOLVER_RULE_PKG";
            }
            case (SOLVER_RULE_PKG_NOT_INSTALLABLE):
            {
                return "SOLVER_RULE_PKG_NOT_INSTALLABLE";
            }
            case (SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP):
            {
                return "SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP";
            }
            case (SOLVER_RULE_PKG_REQUIRES):
            {
                return "SOLVER_RULE_PKG_REQUIRES";
            }
            case (SOLVER_RULE_PKG_SELF_CONFLICT):
            {
                return "SOLVER_RULE_PKG_SELF_CONFLICT";
            }
            case (SOLVER_RULE_PKG_CONFLICTS):
            {
                return "SOLVER_RULE_PKG_CONFLICTS";
            }
            case (SOLVER_RULE_PKG_SAME_NAME):
            {
                return "SOLVER_RULE_PKG_SAME_NAME";
            }
            case (SOLVER_RULE_PKG_OBSOLETES):
            {
                return "SOLVER_RULE_PKG_OBSOLETES";
            }
            case (SOLVER_RULE_PKG_IMPLICIT_OBSOLETES):
            {
                return "SOLVER_RULE_PKG_IMPLICIT_OBSOLETES";
            }
            case (SOLVER_RULE_PKG_INSTALLED_OBSOLETES):
            {
                return "SOLVER_RULE_PKG_INSTALLED_OBSOLETES";
            }
            case (SOLVER_RULE_PKG_RECOMMENDS):
            {
                return "SOLVER_RULE_PKG_RECOMMENDS";
            }
            case (SOLVER_RULE_PKG_CONSTRAINS):
            {
                return "SOLVER_RULE_PKG_CONSTRAINS";
            }
            case (SOLVER_RULE_UPDATE):
            {
                return "SOLVER_RULE_UPDATE";
            }
            case (SOLVER_RULE_FEATURE):
            {
                return "SOLVER_RULE_FEATURE";
            }
            case (SOLVER_RULE_JOB):
            {
                return "SOLVER_RULE_JOB";
            }
            case (SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP):
            {
                return "SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP";
            }
            case (SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM):
            {
                return "SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM";
            }
            case (SOLVER_RULE_JOB_UNKNOWN_PACKAGE):
            {
                return "SOLVER_RULE_JOB_UNKNOWN_PACKAGE";
            }
            case (SOLVER_RULE_JOB_UNSUPPORTED):
            {
                return "SOLVER_RULE_JOB_UNSUPPORTED";
            }
            case (SOLVER_RULE_DISTUPGRADE):
            {
                return "SOLVER_RULE_DISTUPGRADE";
            }
            case (SOLVER_RULE_INFARCH):
            {
                return "SOLVER_RULE_INFARCH";
            }
            case (SOLVER_RULE_CHOICE):
            {
                return "SOLVER_RULE_CHOICE";
            }
            case (SOLVER_RULE_LEARNT):
            {
                return "SOLVER_RULE_LEARNT";
            }
            case (SOLVER_RULE_BEST):
            {
                return "SOLVER_RULE_BEST";
            }
            case (SOLVER_RULE_YUMOBS):
            {
                return "SOLVER_RULE_YUMOBS";
            }
            case (SOLVER_RULE_RECOMMENDS):
            {
                return "SOLVER_RULE_RECOMMENDS";
            }
            case (SOLVER_RULE_BLACK):
            {
                return "SOLVER_RULE_BLACK";
            }
            case (SOLVER_RULE_STRICT_REPO_PRIORITY):
            {
                return "SOLVER_RULE_STRICT_REPO_PRIORITY";
            }
            default:
            {
                throw std::runtime_error("Invalid SolverRuleinfo: " + std::to_string(rule));
            }
        }
    }

    MSolver::MSolver(MPool pool, const std::vector<std::pair<int, int>> flags)
        : m_flags(std::move(flags))
        , m_is_solved(false)
        , m_pool(std::move(pool))
        , m_solver(nullptr)
        , m_jobs(std::make_unique<solv::ObjQueue>())
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
        Pool* const pool = m_pool;
        if (pool->installed == nullptr)
        {
            throw std::runtime_error("Did not find any packages marked as installed.");
        }

        // 1. check if spec is already installed
        Id needle = pool_str2id(m_pool, ms.name.c_str(), 0);
        static Id real_repo_key = pool_str2id(pool, "solvable:real_repo_url", 1);

        if (needle && (pool->installed != nullptr))
        {
            Id pkg_id;
            Solvable* s;
            FOR_REPO_SOLVABLES(pool->installed, pkg_id, s)
            {
                if (s->name == needle)
                {
                    // the data about the channel is only in the prefix_data unfortunately

                    std::string selected_channel;
                    if (solvable_lookup_str(s, real_repo_key))
                    {
                        // this is the _full_ url to the file (incl. abc.tar.bz2)
                        selected_channel = solvable_lookup_str(s, real_repo_key);
                    }
                    else
                    {
                        throw std::runtime_error(
                            "Could not find channel associated with reinstall package"
                        );
                    }

                    selected_channel = m_pool.channel_context().make_channel(selected_channel).name();

                    MatchSpec modified_spec(ms);
                    if (!ms.channel.empty() || !ms.version.empty() || !ms.build_string.empty())
                    {
                        Console::stream() << ms.conda_build_form()
                                          << ": overriding channel, version and build from "
                                             "installed packages due to --force-reinstall.";
                        ms.channel = "";
                        ms.version = "";
                        ms.build_string = "";
                    }

                    modified_spec.channel = selected_channel;
                    modified_spec.version = raw_str_or_empty(pool_id2str(pool, s->evr));
                    modified_spec.build_string = raw_str_or_empty(
                        solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR)
                    );
                    LOG_INFO << "Reinstall " << modified_spec.conda_build_form() << " from channel "
                             << selected_channel;
                    m_jobs->push_back(
                        job_flag | SOLVER_SOLVABLE_PROVIDES,
                        m_pool.matchspec2id(modified_spec)
                    );
                    return;
                }
            }
        }
        m_jobs->push_back(job_flag | SOLVER_SOLVABLE_PROVIDES, m_pool.matchspec2id(ms));
    }

    void MSolver::add_jobs(const std::vector<std::string>& jobs, int job_flag)
    {
        for (const auto& job : jobs)
        {
            MatchSpec ms{ job, m_pool.channel_context() };
            int job_type = job_flag & SOLVER_JOBMASK;

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

            ::Id const job_id = m_pool.matchspec2id(ms);

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
            else if ((job_flag & SOLVER_INSTALL) && force_reinstall)
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

        ::Pool* pool = m_pool;
        ::Repo* const installed_repo = pool->installed;

        if (pool->disttype != DISTTYPE_CONDA)
        {
            throw std::runtime_error("Cannot add pin to a pool that is not of Conda distype");
        }
        if (installed_repo == nullptr)
        {
            throw std::runtime_error("Cannot add pin without a repo of installed packages");
        }

        // Add dummy solvable with a constraint on the pin (not installed if not present)
        ::Id const cons_solv_id = repo_add_solvable(installed_repo);
        ::Solvable* const cons_solv = pool_id2solvable(pool, cons_solv_id);
        // TODO set some "pin" key on the solvable so that we can retrieve it during error messages
        std::string const cons_solv_name = fmt::format("pin-{}", m_pinned_specs.size());
        solvable_set_str(cons_solv, SOLVABLE_NAME, cons_solv_name.c_str());
        solvable_set_str(cons_solv, SOLVABLE_EVR, "1");
        ::Id const pin_ms_id = m_pool.matchspec2id(pin_ms);
        solv::ObjQueue q = { pin_ms_id };
        solvable_add_idarray(cons_solv, SOLVABLE_CONSTRAINS, pin_ms_id);
        // Solvable need to provide itself
        cons_solv->provides = repo_addid_dep(
            installed_repo,
            cons_solv->provides,
            pool_rel2id(pool, cons_solv->name, cons_solv->evr, REL_EQ, 1),
            0
        );

        // Necessary for attributes to be properly stored
        repo_internalize(installed_repo);

        // Lock the dummy solvable so that it stays install.
        add_jobs({ cons_solv_name }, SOLVER_LOCK);
        // Force check the dummy solvable dependencies, as this is not the default for
        // installed packges.
        add_jobs({ cons_solv_name }, SOLVER_VERIFY);
    }

    void MSolver::add_pins(const std::vector<std::string>& pins)
    {
        for (auto pin : pins)
        {
            add_pin(pin);
        }
    }

    void MSolver::set_postsolve_flags(const std::vector<std::pair<int, int>>& flags)
    {
        for (const auto& option : flags)
        {
            switch (option.first)
            {
                case MAMBA_NO_DEPS:
                    no_deps = option.second;
                    break;
                case MAMBA_ONLY_DEPS:
                    only_deps = option.second;
                    break;
                case MAMBA_FORCE_REINSTALL:
                    force_reinstall = option.second;
                    break;
            }
        }
    }

    void MSolver::set_flags(const std::vector<std::pair<int, int>>& flags)
    {
        // TODO use new API
        for (const auto& option : flags)
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
        set_flags(m_flags);

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
        const auto& ctx = Context::instance();
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
            { problems << "  - " << solver().problem_to_string(m_pool.pool(), pb); }
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
            using conflicts_t = ProblemsGraph::conflicts_t;

            ProblemsGraphCreator(const MSolver& solver, const MPool& pool);

            ProblemsGraph problem_graph() &&;

        private:

            const MSolver& m_solver;
            const MPool& m_pool;
            graph_t m_graph;
            conflicts_t m_conflicts;
            std::map<SolvId, node_id> m_solv2node;
            node_id m_root_node;

            /**
             * Add a node and return the node id.
             *
             * If the node is already present and ``update`` is false then the current
             * node is left as it is, otherwise the new value is inserted.
             */
            node_id add_solvable(SolvId solv_id, node_t&& pkg_info, bool update = true);

            void add_conflict(node_id n1, node_id n2);
            [[nodiscard]] bool
            add_expanded_deps_edges(node_id from_id, SolvId dep_id, const edge_t& edge);

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

        auto ProblemsGraphCreator::add_solvable(SolvId solv_id, node_t&& node, bool update) -> node_id
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

        bool
        ProblemsGraphCreator::add_expanded_deps_edges(node_id from_id, SolvId dep_id, const edge_t& edge)
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
                                        << solver_ruleinfo_name(type);
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
                                        << solver_ruleinfo_name(type);
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
                        // Encounterd in the problems list from libsolv but unknown.
                        // Explicitly ignored until we do something with it.
                        break;
                    }
                    default:
                    {
                        // Many more SolverRuleinfo that heve not been encountered.
                        LOG_WARNING << "Problem type not implemented " << solver_ruleinfo_name(type);
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
