#ifndef MAMBA_SOLVER_HPP
#define MAMBA_SOLVER_HPP

#include <vector>
#include <utility>
#include <string>
#include <sstream>

#include "pool.hpp"
#include "prefix_data.hpp"
#include "match_spec.hpp"
#include "output.hpp"

extern "C"
{
    #include "solv/queue.h"
    #include "solv/conda.h"
    #include "solv/solver.h"
    #include "solv/solverdebug.h"
}

#define MAMBA_NO_DEPS         0b0001
#define MAMBA_ONLY_DEPS       0b0010
#define MAMBA_FORCE_REINSTALL 0b0100

namespace mamba
{
    class MSolver
    {
    public:

        MSolver(MPool& pool, const std::vector<std::pair<int, int>>& flags = {});
        MSolver(MPool& pool, const std::vector<std::pair<int, int>>& flags, const PrefixData& prefix_data);
        ~MSolver();

        MSolver(const MSolver&) = delete;
        MSolver& operator=(const MSolver&) = delete;
        MSolver(MSolver&&) = delete;
        MSolver& operator=(MSolver&&) = delete;

        void add_jobs(const std::vector<std::string>& jobs, int job_flag);
        void add_constraint(const std::string& job);
        void set_flags(const std::vector<std::pair<int, int>>& flags);
        void set_postsolve_flags(const std::vector<std::pair<int, int>>& flags);
        bool is_solved();
        bool solve();
        std::string problems_to_str();

        const std::vector<MatchSpec>& install_specs() const;
        const std::vector<MatchSpec>& remove_specs() const;

        operator Solver*();

        bool only_deps = false;
        bool no_deps = false;
        bool force_reinstall = false;

    private:

        void add_channel_specific_job(const MatchSpec& ms, int job_flag);
        void add_reinstall_job(const MatchSpec& ms, int job_flag);

        std::vector<std::pair<int, int>> m_flags;
        std::vector<MatchSpec> m_install_specs;
        std::vector<MatchSpec> m_remove_specs;
        std::vector<MatchSpec> m_neuter_specs; // unused for now
        bool m_is_solved;
        Solver* m_solver;
        Pool* m_pool;
        Queue m_jobs;
        const PrefixData* m_prefix_data = nullptr;
    };
}

#endif // MAMBA_SOLVER_HPP
