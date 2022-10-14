// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>

#include "mamba/core/solver.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/channel.hpp"
#include "mamba/core/match_spec.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/repo.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{

    // TODO this should belong in libsolv.
    char const* solver_ruleinfo_name(SolverRuleinfo rule)
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

    std::string MSolverProblem::to_string() const
    {
        return solver_problemruleinfo2str(
            solver, (SolverRuleinfo) type, source_id, target_id, dep_id);
    }

    std::optional<PackageInfo> MSolverProblem::target() const
    {
        if (target_id == 0 || target_id >= solver->pool->nsolvables)
            return std::nullopt;
        return pool_id2solvable(solver->pool, target_id);
    }

    std::optional<PackageInfo> MSolverProblem::source() const
    {
        if (source_id == 0 || source_id >= solver->pool->nsolvables)
            return std::nullopt;
        ;
        return pool_id2solvable(solver->pool, source_id);
    }

    std::optional<std::string> MSolverProblem::dep() const
    {
        if (!dep_id)
            return std::nullopt;
        return pool_dep2str(solver->pool, dep_id);
    }

    MSolver::MSolver(MPool& pool, const std::vector<std::pair<int, int>>& flags)
        : m_flags(flags)
        , m_is_solved(false)
        , m_solver(nullptr)
        , m_pool(pool)
    {
        queue_init(&m_jobs);
        pool_createwhatprovides(pool);
    }

    MSolver::~MSolver()
    {
        LOG_INFO << "Freeing solver.";
        if (m_solver != nullptr)
        {
            solver_free(m_solver);
        }
    }

    inline bool channel_match(Solvable* s, const Channel& needle)
    {
        MRepo* mrepo = reinterpret_cast<MRepo*>(s->repo->appdata);
        const Channel* chan = mrepo->channel();

        if (!chan)
            return false;

        if ((*chan) == needle)
            return true;

        auto& custom_multichannels = Context::instance().custom_multichannels;
        auto x = custom_multichannels.find(needle.name());
        if (x != custom_multichannels.end())
        {
            for (auto el : (x->second))
            {
                const Channel& inner = make_channel(el);
                if ((*chan) == inner)
                    return true;
            }
        }

        return false;
    }

    void MSolver::add_global_job(int job_flag)
    {
        queue_push2(&m_jobs, job_flag, 0);
    }

    void MSolver::add_channel_specific_job(const MatchSpec& ms, int job_flag)
    {
        Pool* pool = m_pool;
        Queue selected_pkgs;
        queue_init(&selected_pkgs);

        // conda_build_form does **NOT** contain the channel info
        Id match = pool_conda_matchspec(pool, ms.conda_build_form().c_str());

        const Channel& c = make_channel(ms.channel);
        for (Id* wp = pool_whatprovides_ptr(pool, match); *wp; wp++)
        {
            if (channel_match(pool_id2solvable(pool, *wp), c))
            {
                queue_push(&selected_pkgs, *wp);
            }
        }
        if (selected_pkgs.count == 0)
        {
            LOG_ERROR << "Selected channel specific (or force-reinstall) job, but "
                         "package is not available from channel. Solve job will fail.";
        }
        Id d = pool_queuetowhatprovides(pool, &selected_pkgs);
        queue_push2(&m_jobs, job_flag | SOLVER_SOLVABLE_ONE_OF, d);
        queue_free(&selected_pkgs);
    }

    void MSolver::add_reinstall_job(MatchSpec& ms, int job_flag)
    {
        if (!m_pool->installed)
        {
            throw std::runtime_error("Did not find any packages marked as installed.");
        }

        Pool* pool = m_pool;

        // 1. check if spec is already installed
        Id needle = pool_str2id(m_pool, ms.name.c_str(), 0);
        static Id real_repo_key = pool_str2id(pool, "solvable:real_repo_url", 1);

        if (needle && m_pool->installed)
        {
            Id pkg_id;
            Solvable* s;
            FOR_REPO_SOLVABLES(m_pool->installed, pkg_id, s)
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
                            "Could not find channel associated with reinstall package");
                    }

                    selected_channel = make_channel(selected_channel).name();

                    MatchSpec modified_spec(ms);
                    if (!ms.channel.empty() || !ms.version.empty() || !ms.build.empty())
                    {
                        Console::stream() << ms.conda_build_form()
                                          << ": overriding channel, version and build from "
                                             "installed packages due to --force-reinstall.";
                        ms.channel = "";
                        ms.version = "";
                        ms.build = "";
                    }

                    modified_spec.channel = selected_channel;
                    modified_spec.version = check_char(pool_id2str(pool, s->evr));
                    modified_spec.build = check_char(solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR));
                    LOG_INFO << "Reinstall " << modified_spec.conda_build_form() << " from channel "
                             << selected_channel;
                    return add_channel_specific_job(modified_spec, job_flag);
                }
            }
        }
        Id inst_id
            = pool_conda_matchspec(reinterpret_cast<Pool*>(m_pool), ms.conda_build_form().c_str());
        queue_push2(&m_jobs, job_flag | SOLVER_SOLVABLE_PROVIDES, inst_id);
    }

    void MSolver::add_jobs(const std::vector<std::string>& jobs, int job_flag)
    {
        for (const auto& job : jobs)
        {
            MatchSpec ms(job);
            int job_type = job_flag & SOLVER_JOBMASK;

            if (job_type & SOLVER_INSTALL)
                m_install_specs.emplace_back(job);
            else if (job_type == SOLVER_ERASE)
                m_remove_specs.emplace_back(job);
            else if (job_type == SOLVER_LOCK)
                m_neuter_specs.emplace_back(job);  // not used for the moment

            // This is checking if SOLVER_ERASE and SOLVER_INSTALL are set
            // which are the flags for SOLVER_UPDATE
            if (((job_flag & SOLVER_UPDATE) ^ SOLVER_UPDATE) == 0)
            {
                // ignoring update specs here for now
                if (!ms.is_simple())
                {
                    Id inst_id = pool_conda_matchspec(reinterpret_cast<Pool*>(m_pool),
                                                      ms.conda_build_form().c_str());
                    queue_push2(&m_jobs, SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, inst_id);
                }
                if (ms.channel.empty())
                {
                    Id update_id
                        = pool_conda_matchspec(reinterpret_cast<Pool*>(m_pool), ms.name.c_str());
                    queue_push2(&m_jobs, job_flag | SOLVER_SOLVABLE_PROVIDES, update_id);
                }
                else
                    add_channel_specific_job(ms, job_flag);

                continue;
            }

            if (!ms.channel.empty())
            {
                if (job_type == SOLVER_ERASE)
                {
                    throw std::runtime_error("Cannot remove channel-specific spec '" + job + "'");
                }
                add_channel_specific_job(ms, job_flag);
            }
            else if (job_flag & SOLVER_INSTALL && force_reinstall)
            {
                add_reinstall_job(ms, job_flag);
            }
            else
            {
                // Todo remove double parsing?
                LOG_INFO << "Adding job: " << ms.conda_build_form();
                Id inst_id = pool_conda_matchspec(reinterpret_cast<Pool*>(m_pool),
                                                  ms.conda_build_form().c_str());
                queue_push2(&m_jobs, job_flag | SOLVER_SOLVABLE_PROVIDES, inst_id);
            }
        }
    }

    void MSolver::add_constraint(const std::string& job)
    {
        MatchSpec ms(job);
        Id inst_id
            = pool_conda_matchspec(reinterpret_cast<Pool*>(m_pool), ms.conda_build_form().c_str());
        queue_push2(&m_jobs, SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, inst_id);
    }

    void MSolver::add_pin(const std::string& pin)
    {
        // if we pin a package, we need to remove all packages that don't match the
        // pin from being available for installation! This is done by adding
        // SOLVER_LOCK to the packages, so that they are prevented from being
        // installed A lock basically says: keep the state of the package. I.e.
        // uninstalled packages stay uninstalled, installed packages stay installed.
        // A lock is a hard requirement, we could also use SOLVER_FAVOR for soft
        // requirements

        // First we need to check if the pin is OK given the currently installed
        // packages
        Pool* pool = m_pool;
        MatchSpec ms(pin);

        // TODO
        // if (m_prefix_data)
        // {
        //     for (auto& [name, record] : m_prefix_data->records())
        //     {
        //         LOG_ERROR << "NAME " << name;
        //         if (name == ms.name)
        //         {
        //             LOG_ERROR << "Found pinned package in installed packages, need
        //             to check pin now."; LOG_ERROR << record.version << " vs " <<
        //             ms.version;
        //         }
        //     }
        // }

        Id match = pool_conda_matchspec(pool, ms.conda_build_form().c_str());

        std::set<Id> matching_solvables;
        const Channel& c = make_channel(ms.channel);

        for (Id* wp = pool_whatprovides_ptr(pool, match); *wp; wp++)
        {
            if (!ms.channel.empty())
            {
                if (!channel_match(pool_id2solvable(pool, *wp), c))
                {
                    continue;
                }
            }
            matching_solvables.insert(*wp);
        }

        std::set<Id> all_solvables;
        Id name_id = pool_str2id(pool, ms.name.c_str(), 1);
        for (Id* wp = pool_whatprovides_ptr(pool, name_id); *wp; wp++)
        {
            all_solvables.insert(*wp);
        }

        if (all_solvables.size() != 0 && matching_solvables.size() == 0)
        {
            throw std::runtime_error(fmt::format("No package can be installed for pin: {}", pin));
        }
        m_pinned_specs.push_back(ms);

        Queue selected_pkgs;
        queue_init(&selected_pkgs);

        for (auto& id : all_solvables)
        {
            if (matching_solvables.find(id) == matching_solvables.end())
            {
                // the solvable is _NOT_ matched by our pinning expression! So we have to
                // lock it to make it un-installable
                queue_push(&selected_pkgs, id);
            }
        }

        Id d = pool_queuetowhatprovides(pool, &selected_pkgs);
        queue_push2(&m_jobs, SOLVER_LOCK | SOLVER_SOLVABLE_ONE_OF, d);
        queue_free(&selected_pkgs);
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
            solver_set_flag(m_solver, option.first, option.second);
        }
    }

    bool MSolver::is_solved() const
    {
        return m_is_solved;
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

    bool MSolver::solve()
    {
        m_solver = solver_create(m_pool);
        set_flags(m_flags);

        solver_solve(m_solver, &m_jobs);
        m_is_solved = true;
        const auto problem_count = solver_problem_count(m_solver);
        LOG_INFO << "Problem count: " << problem_count;
        const bool success = problem_count == 0;
        Console::instance().json_write({ { "success", success } });
        return success;
    }


    std::vector<MSolverProblem> MSolver::all_problems_structured() const
    {
        std::vector<MSolverProblem> res;
        Queue problem_rules;
        queue_init(&problem_rules);
        Id count = solver_problem_count(m_solver);
        for (Id i = 1; i <= count; ++i)
        {
            solver_findallproblemrules(m_solver, i, &problem_rules);
            for (Id j = 0; j < problem_rules.count; ++j)
            {
                Id type, source, target, dep;
                Id r = problem_rules.elements[j];
                if (r)
                {
                    type = solver_ruleinfo(m_solver, r, &source, &target, &dep);
                    MSolverProblem problem;
                    problem.source_id = source;
                    problem.target_id = target;
                    problem.dep_id = dep;
                    problem.solver = m_solver;
                    problem.type = static_cast<SolverRuleinfo>(type);
                    res.push_back(problem);
                }
            }
        }
        queue_free(&problem_rules);
        return res;
    }


    std::string MSolver::all_problems_to_str() const
    {
        std::stringstream problems;

        Queue problem_rules;
        queue_init(&problem_rules);
        Id count = solver_problem_count(m_solver);
        for (Id i = 1; i <= count; ++i)
        {
            solver_findallproblemrules(m_solver, i, &problem_rules);
            for (Id j = 0; j < problem_rules.count; ++j)
            {
                Id type, source, target, dep;
                Id r = problem_rules.elements[j];
                if (!r)
                {
                    problems << "- [SKIP] no problem rule?\n";
                }
                else
                {
                    type = solver_ruleinfo(m_solver, r, &source, &target, &dep);
                    problems << "  - "
                             << solver_problemruleinfo2str(
                                    m_solver, (SolverRuleinfo) type, source, target, dep)
                             << "\n";
                }
            }
        }
        queue_free(&problem_rules);
        return problems.str();
    }

    std::string MSolver::problems_to_str() const
    {
        Queue problem_queue;
        queue_init(&problem_queue);
        int count = solver_problem_count(m_solver);
        std::stringstream problems;
        for (int i = 1; i <= count; i++)
        {
            queue_push(&problem_queue, i);
            problems << "  - " << solver_problem2str(m_solver, i) << "\n";
        }
        queue_free(&problem_queue);
        return "Encountered problems while solving:\n" + problems.str();
    }

    std::vector<std::string> MSolver::all_problems() const
    {
        std::vector<std::string> problems;
        Queue problem_queue;
        queue_init(&problem_queue);
        int count = static_cast<int>(solver_problem_count(m_solver));
        for (int i = 1; i <= count; i++)
        {
            queue_push(&problem_queue, i);
            problems.emplace_back(solver_problem2str(m_solver, i));
        }
        queue_free(&problem_queue);

        return problems;
    }

    MSolver::operator Solver*()
    {
        return m_solver;
    }
}  // namespace mamba
