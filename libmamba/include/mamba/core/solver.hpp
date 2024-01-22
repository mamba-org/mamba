// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SOLVER_HPP
#define MAMBA_CORE_SOLVER_HPP

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <solv/pooltypes.h>
#include <solv/solvable.h>
// Incomplete header
#include <solv/rules.h>

#include "mamba/core/pool.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba::solv
{
    class ObjQueue;
    class ObjSolver;
}

namespace mamba
{

    struct SolverProblem
    {
        SolverRuleinfo type;
        Id source_id;
        Id target_id;
        Id dep_id;
        std::optional<specs::PackageInfo> source;
        std::optional<specs::PackageInfo> target;
        std::optional<std::string> dep;
        std::string description;
    };

    struct Request
    {
        struct Install
        {
            specs::MatchSpec spec;
        };

        struct Remove
        {
            specs::MatchSpec spec;
            bool clean_dependencies = true;
        };

        struct Update
        {
            specs::MatchSpec spec;
        };

        struct UpdateAll
        {
            bool clean_dependencies = true;
        };

        struct Keep
        {
            specs::MatchSpec spec;
        };

        struct Freeze
        {
            specs::MatchSpec spec;
        };

        struct Pin
        {
            specs::MatchSpec spec;
        };

        using Item = std::variant<Install, Remove, Update, UpdateAll, Keep, Freeze, Pin>;
        using item_list = std::vector<Item>;

        item_list items = {};
    };

    class MSolver
    {
    public:

        struct Flags
        {
            /** Keep the dependencies of the install package in the solution. */
            bool keep_dependencies = true;
            /** Keep the original required package in the solution. */
            bool keep_specs = true;
            /** Force reinstallation of jobs. */
            bool force_reinstall = false;
        };

        MSolver(MPool pool, std::vector<std::pair<int, int>> flags = {});
        ~MSolver();

        MSolver(const MSolver&) = delete;
        MSolver& operator=(const MSolver&) = delete;
        MSolver(MSolver&&);
        MSolver& operator=(MSolver&&);

        void add_request(const Request& request);

        void set_flags(const Flags& flags);  // TODO temporary Itf meant to be passed in ctor
        [[nodiscard]] auto flags() const -> const Flags&;
        [[deprecated]] void py_set_libsolv_flags(const std::vector<std::pair<int, int>>& flags);

        [[nodiscard]] bool try_solve();
        void must_solve();
        [[nodiscard]] bool is_solved() const;

        [[nodiscard]] std::string problems_to_str() const;
        [[nodiscard]] std::vector<std::string> all_problems() const;
        [[nodiscard]] std::vector<SolverProblem> all_problems_structured() const;
        [[nodiscard]] ProblemsGraph problems_graph() const;
        [[nodiscard]] std::string all_problems_to_str() const;
        std::ostream& explain_problems(std::ostream& out) const;
        [[nodiscard]] std::string explain_problems() const;

        [[nodiscard]] const MPool& pool() const&;
        [[nodiscard]] MPool& pool() &;
        [[nodiscard]] MPool&& pool() &&;

        [[nodiscard]] const std::vector<specs::MatchSpec>& install_specs() const;
        [[nodiscard]] const std::vector<specs::MatchSpec>& remove_specs() const;
        [[nodiscard]] const std::vector<specs::MatchSpec>& neuter_specs() const;
        [[nodiscard]] const std::vector<specs::MatchSpec>& pinned_specs() const;

        auto solver() -> solv::ObjSolver&;
        auto solver() const -> const solv::ObjSolver&;

    private:

        std::vector<std::pair<int, int>> m_libsolv_flags;
        std::vector<specs::MatchSpec> m_install_specs;
        std::vector<specs::MatchSpec> m_remove_specs;
        std::vector<specs::MatchSpec> m_neuter_specs;
        std::vector<specs::MatchSpec> m_pinned_specs;
        // Order of m_pool and m_solver is critical since m_pool must outlive m_solver.
        MPool m_pool;
        // Temporary Pimpl all libsolv to keep it private
        std::unique_ptr<solv::ObjSolver> m_solver;
        std::unique_ptr<solv::ObjQueue> m_jobs;
        Flags m_flags = {};
        bool m_is_solved;

        void add_reinstall_job(const specs::MatchSpec& ms, int job_flag);
        void apply_libsolv_flags();

        void add_job_impl(const Request::Install& job);
        void add_job_impl(const Request::Remove& job);
        void add_job_impl(const Request::Update& job);
        void add_job_impl(const Request::UpdateAll& job);
        void add_job_impl(const Request::Freeze& job);
        void add_job_impl(const Request::Keep& job);
        void add_job_impl(const Request::Pin& job);
    };
}  // namespace mamba

#endif  // MAMBA_SOLVER_HPP
