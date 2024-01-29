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
#include <vector>

#include <solv/pooltypes.h>
#include <solv/solvable.h>
// Incomplete header
#include <solv/rules.h>

#include "mamba/core/pool.hpp"
#include "mamba/core/satisfiability_error.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba::solv
{
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

    class MSolver
    {
    public:

        using Request = solver::Request;

        MSolver(MPool pool);
        ~MSolver();

        MSolver(const MSolver&) = delete;
        MSolver& operator=(const MSolver&) = delete;
        MSolver(MSolver&&);
        MSolver& operator=(MSolver&&);

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

        void set_request(Request request);
        [[nodiscard]] const Request& request() const;

        auto solver() -> solv::ObjSolver&;
        auto solver() const -> const solv::ObjSolver&;

    private:

        Request m_request;
        // Order of m_pool and m_solver is critical since m_pool must outlive m_solver.
        MPool m_pool;
        // Temporary Pimpl all libsolv to keep it private
        std::unique_ptr<solv::ObjSolver> m_solver;
        bool m_is_solved;

        void apply_libsolv_flags();
    };
}  // namespace mamba

#endif  // MAMBA_SOLVER_HPP
