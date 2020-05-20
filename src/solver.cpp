#include "solver.hpp"
#include "output.hpp"

namespace mamba
{
    MSolver::MSolver(MPool& pool, const std::vector<std::pair<int, int>>& flags)
        : m_is_solved(false)
    {
        m_solver = solver_create(pool);
        set_flags(flags);
        queue_init(&m_jobs);
    }

    MSolver::~MSolver()
    {
        LOG_INFO << "Freeing solver.";
        solver_free(m_solver);
    }

    void MSolver::add_jobs(const std::vector<std::string>& jobs, int job_flag)
    {
        for (const auto& job : jobs)
        {
            Id inst_id = pool_conda_matchspec(m_solver->pool, job.c_str());
            if (job_flag & SOLVER_INSTALL)
            {
                m_install_specs.push_back(job);
            }
            if (job_flag & SOLVER_ERASE)
            {
                m_remove_specs.push_back(job);
            }
            queue_push2(&m_jobs, job_flag | SOLVER_SOLVABLE_PROVIDES, inst_id);
        }
    }

    void MSolver::add_constraint(const std::string& job)
    {
        Id inst_id = pool_conda_matchspec(m_solver->pool, job.c_str());
        queue_push2(&m_jobs, SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES, inst_id);
    }

    void MSolver::set_postsolve_flags(const std::vector<std::pair<int, int>>& flags)
    {
        for (const auto& option : flags)
        {
            switch (option.first)
            {
                case MAMBA_NO_DEPS:
                    no_deps = option.second; break;
                case MAMBA_ONLY_DEPS:
                    only_deps = option.second; break;
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

    bool MSolver::solve()
    {
        bool success;
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
