// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SOLVER_HPP
#define MAMBA_CORE_SOLVER_HPP

#include <string>
#include <utility>
#include <vector>
#include <optional>

#include "match_spec.hpp"

extern "C"
{
#include "solv/queue.h"
#include "solv/solver.h"
}

#define MAMBA_NO_DEPS 0b0001
#define MAMBA_ONLY_DEPS 0b0010
#define MAMBA_FORCE_REINSTALL 0b0100

namespace mamba
{

    char const* solver_ruleinfo_name(SolverRuleinfo rule);

    class MPool;
    class PackageInfo;

    class MSolverProblem
    {
    public:
        SolverRuleinfo type;
        Id source_id;
        Id target_id;
        Id dep_id;

        Solver* solver;

        std::string to_string() const;

        std::optional<PackageInfo> target() const;
        std::optional<PackageInfo> source() const;
        std::optional<std::string> dep() const;
    };

    class MSolver
    {
    public:
        MSolver(MPool& pool, const std::vector<std::pair<int, int>>& flags = {});
        ~MSolver();

        MSolver(const MSolver&) = delete;
        MSolver& operator=(const MSolver&) = delete;
        MSolver(MSolver&&) = delete;
        MSolver& operator=(MSolver&&) = delete;

        void add_global_job(int job_flag);
        void add_jobs(const std::vector<std::string>& jobs, int job_flag);
        void add_constraint(const std::string& job);
        void add_pin(const std::string& pin);
        void add_pins(const std::vector<std::string>& pins);
        void set_flags(const std::vector<std::pair<int, int>>& flags);
        void set_postsolve_flags(const std::vector<std::pair<int, int>>& flags);
        bool is_solved() const;
        bool solve();
        std::string problems_to_str() const;
        std::vector<std::string> all_problems() const;
        std::vector<MSolverProblem> all_problems_structured() const;
        std::string all_problems_to_str() const;

        const std::vector<MatchSpec>& install_specs() const;
        const std::vector<MatchSpec>& remove_specs() const;
        const std::vector<MatchSpec>& neuter_specs() const;
        const std::vector<MatchSpec>& pinned_specs() const;

        operator Solver*();

        bool only_deps = false;
        bool no_deps = false;
        bool force_reinstall = false;

    private:
        void add_channel_specific_job(const MatchSpec& ms, int job_flag);
        void add_reinstall_job(MatchSpec& ms, int job_flag);

        std::vector<std::pair<int, int>> m_flags;
        std::vector<MatchSpec> m_install_specs;
        std::vector<MatchSpec> m_remove_specs;
        std::vector<MatchSpec> m_neuter_specs;
        std::vector<MatchSpec> m_pinned_specs;
        bool m_is_solved;
        Solver* m_solver;
        Pool* m_pool;
        Queue m_jobs;
    };
}  // namespace mamba

#endif  // MAMBA_SOLVER_HPP
