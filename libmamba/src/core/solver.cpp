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
#include "solv-cpp/queue.hpp"

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

    namespace
    {
        void delete_libsolve_solver(::Solver* solver)
        {
            LOG_INFO << "Freeing solver.";
            if (solver != nullptr)
            {
                solver_free(solver);
            }
        }

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

    MSolver::MSolver(MPool pool, const std::vector<std::pair<int, int>> flags)
        : m_flags(std::move(flags))
        , m_is_solved(false)
        , m_pool(std::move(pool))
        , m_solver(nullptr, &delete_libsolve_solver)
        , m_jobs(std::make_unique<solv::ObjQueue>())
    {
        pool_createwhatprovides(m_pool);
    }

    MSolver::~MSolver() = default;

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

                    selected_channel = make_channel(selected_channel).name();

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
            MatchSpec ms(job);
            int job_type = job_flag & SOLVER_JOBMASK;

            if (job_type & SOLVER_INSTALL)
            {
                m_install_specs.emplace_back(job);
            }
            else if (job_type == SOLVER_ERASE)
            {
                m_remove_specs.emplace_back(job);
            }
            else if (job_type == SOLVER_LOCK)
            {
                m_neuter_specs.emplace_back(job);  // not used for the moment
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
        m_jobs->push_back(SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, m_pool.matchspec2id({ job }));
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

        const auto pin_ms = MatchSpec(pin);
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
        for (const auto& option : flags)
        {
            solver_set_flag(m_solver.get(), option.first, option.second);
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
        m_solver.reset(solver_create(m_pool));
        set_flags(m_flags);

        solver_solve(m_solver.get(), m_jobs->raw());
        m_is_solved = true;
        LOG_INFO << "Problem count: " << solver_problem_count(m_solver.get());
        const bool success = solver_problem_count(m_solver.get()) == 0;
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

    std::vector<MSolverProblem> MSolver::all_problems_structured() const
    {
        std::vector<MSolverProblem> res;
        solv::ObjQueue problem_rules;
        const auto count = static_cast<Id>(solver_problem_count(m_solver.get()));
        for (Id i = 1; i <= count; ++i)
        {
            solver_findallproblemrules(m_solver.get(), i, problem_rules.raw());
            for (const Id r : problem_rules)
            {
                if (r != 0)
                {
                    Id source, target, dep;
                    const SolverRuleinfo type = solver_ruleinfo(m_solver.get(), r, &source, &target, &dep);
                    res.push_back(make_solver_problem(
                        /* solver= */ *this,
                        /* pool= */ m_pool,
                        /* type= */ type,
                        /* source_id= */ source,
                        /* target_id= */ target,
                        /* dep_id= */ dep
                    ));
                }
            }
        }
        return res;
    }


    std::string MSolver::all_problems_to_str() const
    {
        std::stringstream problems;

        solv::ObjQueue problem_rules;
        auto count = static_cast<Id>(solver_problem_count(m_solver.get()));
        for (Id i = 1; i <= count; ++i)
        {
            solver_findallproblemrules(m_solver.get(), i, problem_rules.raw());
            for (const Id r : problem_rules)
            {
                Id source, target, dep;
                if (!r)
                {
                    problems << "- [SKIP] no problem rule?\n";
                }
                else
                {
                    const SolverRuleinfo type = solver_ruleinfo(m_solver.get(), r, &source, &target, &dep);
                    problems << "  - "
                             << solver_problemruleinfo2str(m_solver.get(), type, source, target, dep)
                             << "\n";
                }
            }
        }
        return problems.str();
    }

    std::ostream& MSolver::explain_problems(std::ostream& out) const
    {
        const auto& ctx = Context::instance();
        out << "Could not solve for environment specs\n";
        const auto pbs = ProblemsGraph::from_solver(*this, pool());
        const auto pbs_simplified = simplify_conflicts(pbs);
        const auto cp_pbs = CompressedProblemsGraph::from_problems_graph(pbs_simplified);
        print_problem_tree_msg(
            out,
            cp_pbs,
            { /* .unavailable= */ ctx.design_info.palette.failure,
              /* .available= */ ctx.design_info.palette.success }
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
        solv::ObjQueue problem_queue;
        auto count = static_cast<int>(solver_problem_count(m_solver.get()));
        std::stringstream problems;
        for (int i = 1; i <= count; i++)
        {
            problem_queue.push_back(i);
            problems << "  - " << solver_problem2str(m_solver.get(), i) << "\n";
        }
        return "Encountered problems while solving:\n" + problems.str();
    }

    std::vector<std::string> MSolver::all_problems() const
    {
        std::vector<std::string> problems;
        solv::ObjQueue problem_queue;
        int count = static_cast<int>(solver_problem_count(m_solver.get()));
        for (int i = 1; i <= count; i++)
        {
            problem_queue.push_back(i);
            problems.emplace_back(solver_problem2str(m_solver.get(), i));
        }

        return problems;
    }

    MSolver::operator const Solver*() const
    {
        return m_solver.get();
    }

    MSolver::operator Solver*()
    {
        return m_solver.get();
    }
}  // namespace mamba
