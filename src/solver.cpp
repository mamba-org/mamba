#include "solver.hpp"
#include "output.hpp"
#include "package_info.hpp"

namespace mamba
{
    MSolver::MSolver(MPool& pool, const std::vector<std::pair<int, int>>& flags) :
        m_is_solved(false),
        m_pool(pool),
        m_flags(flags),
        m_solver(nullptr)
    {
        queue_init(&m_jobs);
    }

    MSolver::~MSolver()
    {
        LOG_INFO << "Freeing solver.";
        if (m_solver != nullptr)
        {
            solver_free(m_solver);
        }
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
            Id inst_id = pool_conda_matchspec((Pool*) m_pool, ms.conda_build_form().c_str());
            queue_push2(&m_jobs, job_flag | SOLVER_SOLVABLE_PROVIDES, inst_id);
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
        // this function adds a fake channel that promotes contains only explicit specs with highest prio
        Repo* override_repo;
        Pool* pool = m_pool;
        override_repo = repo_create(pool, "override_repo");
        override_repo->priority = 1'000'000;
        override_repo->subpriority = 0;
        Repodata* override_data = repo_add_repodata(override_repo, 0);

        struct CBdata
        {
            Id handle;
            Repodata* data;
        };

        auto solvable_copy_cb = [](void *vcbdata, Solvable *r, Repodata *fromdata, Repokey *key, KeyValue *kv) -> int
        {
            CBdata* data = (CBdata*) vcbdata;
            repodata_set_kv(data->data, data->handle, key->name, key->type, kv);
            return 0;
        };

        auto copydeps = [pool, override_repo](Offset fromoff, Repo* fromrepo) -> Offset
        {
            int cc;
            Id *ida, *from;
            Offset ido;

            if (!fromoff)
                return 0;
            from = fromrepo->idarraydata + fromoff;
            for (ida = from, cc = 0; *ida; ida++, cc++)
                ;
            if (cc == 0)
                return 0;
            ido = repo_reserve_ids(override_repo, 0, cc);
            ida = override_repo->idarraydata + ido;
            memcpy(ida, from, (cc + 1) * sizeof(Id));
            override_repo->idarraysize += cc + 1;
            return ido;
        };

        for (auto& install_spec : m_install_specs)
        {
            if (!install_spec.channel.empty())
            {
                Id pkgname_id = pool_str2id(pool, install_spec.name.c_str(), 0);
                // now we have to do two things:
                //  1. make sure that we're pulling that channel (done in mamba.py)
                //  2. add the specified package in a high prio channel that overrides all others

                Id rid;
                Repo* fromrepo;
                Id real_repo_key = pool_str2id(pool, "solvable:real_repo_url", 1);

                FOR_REPOS(rid, fromrepo)
                {
                    if (std::string(fromrepo->name).find(install_spec.channel) != std::string::npos)
                    {
                        Id pkg_id;
                        Solvable* s;
                        FOR_REPO_SOLVABLES(fromrepo, pkg_id, s)
                        {
                            if (s->name == pkgname_id)
                            {
                                Id new_pkg_handle = repo_add_solvable(override_repo);
                                Solvable* new_pkg_solv = pool_id2solvable(pool, new_pkg_handle);
                                new_pkg_solv->name = s->name;
                                new_pkg_solv->evr = s->evr;
                                new_pkg_solv->arch = s->arch;
                                new_pkg_solv->vendor = s->vendor;

                                new_pkg_solv->provides = copydeps(s->provides, fromrepo);
                                new_pkg_solv->requires = copydeps(s->requires, fromrepo);
                                new_pkg_solv->conflicts = copydeps(s->conflicts, fromrepo);
                                new_pkg_solv->obsoletes = copydeps(s->obsoletes, fromrepo);
                                new_pkg_solv->recommends = copydeps(s->recommends, fromrepo);
                                new_pkg_solv->suggests = copydeps(s->suggests, fromrepo);
                                new_pkg_solv->supplements = copydeps(s->supplements, fromrepo);
                                new_pkg_solv->enhances  = copydeps(s->enhances, fromrepo);

                                CBdata cbdata {
                                    new_pkg_handle,
                                    override_data
                                };

                                Repodata* fromdata;
                                int ix;

                                FOR_REPODATAS(fromrepo, ix, fromdata)
                                {
                                    if (pkg_id >= fromdata->start && pkg_id < fromdata->end)
                                    {
                                        repodata_search(fromdata, pkg_id, 0, 0, solvable_copy_cb, &cbdata);
                                    }
                                }
                                repodata_set_str(override_data, new_pkg_handle, real_repo_key, s->repo->name);
                            }
                        }
                    }
                }
            }
        }
        repodata_internalize(override_data);
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
