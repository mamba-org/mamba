// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SOLVER_HPP
#define MAMBA_CORE_SOLVER_HPP

#include <string>
#include <iosfwd>
#include <utility>
#include <vector>
#include <optional>
#include <memory>

#include <solv/queue.h>
#include <solv/solver.h>

#include "mamba/core/pool.hpp"
#include "mamba/core/package_info.hpp"

#include "match_spec.hpp"

#define MAMBA_NO_DEPS 0b0001
#define MAMBA_ONLY_DEPS 0b0010
#define MAMBA_FORCE_REINSTALL 0b0100

namespace mamba
{

    char const* solver_ruleinfo_name(SolverRuleinfo rule);

    struct MSolverProblem
    {
        SolverRuleinfo type;
        Id source_id;
        Id target_id;
        Id dep_id;
        std::optional<PackageInfo> source;
        std::optional<PackageInfo> target;
        std::optional<std::string> dep;
        std::string description;
    };

    class MSolver
    {
    public:
        MSolver(MPool pool, std::vector<std::pair<int, int>> flags = {});
        ~MSolver() = default;

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
        [[nodiscard]] bool try_solve();
        void must_solve();

        [[nodiscard]] bool is_solved() const;
        [[nodiscard]] std::string problems_to_str() const;
        [[nodiscard]] std::vector<std::string> all_problems() const;
        [[nodiscard]] std::vector<MSolverProblem> all_problems_structured() const;
        [[nodiscard]] std::string all_problems_to_str() const;
        std::ostream& explain_problems(std::ostream& out) const;
        [[nodiscard]] std::string explain_problems() const;

        [[nodiscard]] MPool const& pool() const&;
        [[nodiscard]] MPool& pool() &;
        [[nodiscard]] MPool&& pool() &&;

        [[nodiscard]] const std::vector<MatchSpec>& install_specs() const;
        [[nodiscard]] const std::vector<MatchSpec>& remove_specs() const;
        [[nodiscard]] const std::vector<MatchSpec>& neuter_specs() const;
        [[nodiscard]] const std::vector<MatchSpec>& pinned_specs() const;


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
        // Order of m_pool and m_solver is critical since m_pool must outlive m_solver.
        MPool m_pool;
        std::unique_ptr<::Solver, void (*)(::Solver*)> m_solver;
        Queue m_jobs;
    };
}  // namespace mamba

#endif  // MAMBA_SOLVER_HPP
