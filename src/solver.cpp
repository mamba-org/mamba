#include "solver.hpp"
#include "output.hpp"
#include "util.hpp"
#include "package_info.hpp"

namespace mamba
{
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

    void MSolver::add_channel_specific_job(const MatchSpec& ms)
    {
        Pool* pool = m_pool;
        Queue selected_pkgs;
        queue_init(&selected_pkgs);

        // conda_build_form does **NOT** contain the channel info
        Id match = pool_conda_matchspec(pool, ms.conda_build_form().c_str());

        for (Id* wp = pool_whatprovides_ptr(pool, match); *wp; wp++)
        {
            Solvable* s = pool_id2solvable(pool, *wp);
            // TODO this might match too much (e.g. bioconda would also match bioconda-experimental etc)
            // Note: s->repo->name is the URL of the repo
            // TODO maybe better to check all repos, select pointers, and compare the pointer (s->repo == ptr?)
            if (std::string_view(s->repo->name).find(ms.channel) != std::string_view::npos)
            {
                queue_push(&selected_pkgs, *wp);
            }
        }

        Id d = pool_queuetowhatprovides(pool, &selected_pkgs);
        queue_push2(&m_jobs, SOLVER_INSTALL | SOLVER_SOLVABLE_ONE_OF, d);
        queue_free(&selected_pkgs);
    }

    void MSolver::add_jobs(const std::vector<std::string>& jobs, int job_flag)
    {
        for (const auto& job : jobs)
        {
            if (job_flag & SOLVER_INSTALL)
            {
                m_install_specs.push_back(job);
            }
            if (job_flag & SOLVER_ERASE)
            {
                m_remove_specs.push_back(job);
            }
            MatchSpec ms(job);
            if (!ms.channel.empty())
            {
                if (job_flag & SOLVER_ERASE)
                {
                    throw std::runtime_error("Cannot erase a channel-specific package. (" + job + ")");
                }
                add_channel_specific_job(ms);
            }
            else
            {
                Id inst_id = pool_conda_matchspec((Pool*) m_pool, ms.conda_build_form().c_str());
                queue_push2(&m_jobs, job_flag | SOLVER_SOLVABLE_PROVIDES, inst_id);
            }
        }
    }

    void MSolver::add_constraint(const std::string& job)
    {
        MatchSpec ms(job);
        Id inst_id = pool_conda_matchspec((Pool*) m_pool, ms.conda_build_form().c_str());
        queue_push2(&m_jobs, SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, inst_id);
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

    bool MSolver::is_solved()
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

    void MSolver::preprocess_solve()
    {
        Pool* pool = m_pool;
        if (force_reinstall)
        {
            std::cout << "Forcing reinstall!" << std::endl;
            Id pkg_id;
            Solvable* s;

            for (auto& install_spec : m_install_specs)
            {
                // 1. check if spec is already installed
                std::cout << "Checking " << install_spec.name << std::endl;
                Id needle = pool_str2id(m_pool, install_spec.name.c_str(), 0);
                if (needle && m_pool->installed)
                {
                    FOR_REPO_SOLVABLES(m_pool->installed, pkg_id, s)
                    {
                        if (s->name == needle)
                        {
                            std::cout << "Found installed package, now we need to fix the solvable" << std::endl;
                            std::cout << "PKG ID: " << pkg_id << std::endl;
                            const char* version = pool_id2str(pool, s->evr);
                            const char* build_str = solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR);

                            const char* selected_repo_name = "conda-forge/linux-64";

                            Queue* q1 = new Queue();
                            queue_init(q1);
                            queue_push(q1, pkg_id);
                            Id d2 = pool_queuetowhatprovides(pool, q1);
                            queue_push2(&m_jobs, SOLVER_ERASE | SOLVER_SOLVABLE_ONE_OF, d2);
                            // repo_free_solvable(m_pool->installed, pkg_id,);

                            Id repo_id;
                            Repo* selected_repo;
                            Repo* found_repo = nullptr;
                            // Id real_repo_key = pool_str2id(pool, "solvable:real_repo_url", 1);

                            FOR_REPOS(repo_id, selected_repo)
                            {
                                std::cout << selected_repo->name << std::endl;
                                if (ends_with(selected_repo->name, "conda-forge/linux-64"))
                                {
                                    found_repo = selected_repo;
                                }
                            }
                            std::cout << "Found repo" << selected_repo << std::endl;
                            Queue* q = new Queue();
                            queue_init(q);
                            if (found_repo)
                            {
                                Id other_pkg_id;
                                Solvable* other_solvable;
                                FOR_REPO_SOLVABLES(found_repo, other_pkg_id, other_solvable)
                                {
                                    if (other_solvable->name == needle && other_solvable->evr == s->evr
                                        && strcmp(solvable_lookup_str(s, SOLVABLE_BUILDFLAVOR), 
                                                  solvable_lookup_str(other_solvable, SOLVABLE_BUILDFLAVOR)) == 0)
                                    {
                                        std::cout << "Pushing " << other_pkg_id << " to queue" << std::endl;
                                        queue_push(q, other_pkg_id);
                                    }
                                }
                            }

                            Id d = pool_queuetowhatprovides(pool, q);
                            queue_push2(&m_jobs, SOLVER_INSTALL | SOLVER_SOLVABLE_ONE_OF, d);
                            // queue_free(&q);

                            std::cout << install_spec.name << " " << version << ", " << build_str << std::endl;
                            std::cout << "Channel : " << s->repo->name;

                        }
                    }
                }
            }
        }
        return;
    }

    bool MSolver::solve()
    {
        bool success;
        preprocess_solve();
        m_solver = solver_create(m_pool);
        set_flags(m_flags);

        solver_solve(m_solver, &m_jobs);
        m_is_solved = true;
        LOG_WARNING << "Problem count: " << solver_problem_count(m_solver) << std::endl;
        success = solver_problem_count(m_solver) == 0;
        JsonLogger::instance().json_write({{"success", success}});
        return success;
    }

    std::string MSolver::problems_to_str()
    {
        Queue problem_queue;
        queue_init(&problem_queue);
        int count = solver_problem_count(m_solver);
        std::stringstream problems;
        for (int i = 1; i <= count; i++)
        {
            queue_push(&problem_queue, i);
            problems << "Problem: " << solver_problem2str(m_solver, i) << "\n";
        }
        queue_free(&problem_queue);
        return "Encountered problems while solving.\n" + problems.str();
    }

    MSolver::operator Solver*()
    {
        return m_solver;
    }
}
