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
#include <vector>

#include <solv/pooltypes.h>
// Incomplete header
#include <solv/rules.h>

#include "mamba/core/satisfiability_error.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/solver/solution.hpp"
#include "mamba/specs/package_info.hpp"

namespace mamba::solv
{
    class ObjSolver;
}

namespace mamba
{
    class MPool;

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

    class UnSolvable
    {
    public:

        ~UnSolvable();

        [[nodiscard]] auto problems_to_str(MPool& pool) const -> std::string;
        [[nodiscard]] auto all_problems(MPool& pool) const -> std::vector<std::string>;
        [[nodiscard]] auto all_problems_structured(const MPool& pool) const
            -> std::vector<SolverProblem>;
        [[nodiscard]] auto problems_graph(const MPool& pool) const -> ProblemsGraph;
        [[nodiscard]] auto all_problems_to_str(MPool& pool) const -> std::string;
        auto explain_problems_to(MPool& pool, std::ostream& out) const -> std::ostream&;
        [[nodiscard]] auto explain_problems(MPool& pool) const -> std::string;

    private:

        // Pimpl all libsolv to keep it private
        // We could make it a reference if we consider it is worth keeping the data in the Solver
        // for potential resolve.
        std::unique_ptr<solv::ObjSolver> m_solver;

        explicit UnSolvable(std::unique_ptr<solv::ObjSolver>&& solver);

        [[nodiscard]] auto solver() const -> const solv::ObjSolver&;

        friend class MSolver;
    };

    class MSolver
    {
    public:

        using Request = solver::Request;
        using Solution = solver::Solution;
        using Outcome = std::variant<Solution, UnSolvable>;

        MSolver();
        MSolver(const MSolver&) = delete;
        MSolver(MSolver&&);

        ~MSolver();

        auto operator=(const MSolver&) -> MSolver& = delete;
        auto operator=(MSolver&&) -> MSolver&;

        [[nodiscard]] bool try_solve(MPool& pool);
        void must_solve(MPool& pool);
        [[nodiscard]] bool is_solved() const;


        void set_request(Request request);
        [[nodiscard]] const Request& request() const;

        [[nodiscard]] const Solution& solution() const;
        [[nodiscard]] UnSolvable unsolvable();

        auto solver() -> solv::ObjSolver&;
        auto solver() const -> const solv::ObjSolver&;

    private:

        Request m_request = {};
        Solution m_solution = {};
        // Pimpl all libsolv to keep it private
        std::unique_ptr<solv::ObjSolver> m_solver;
        bool m_is_solved = false;
    };
}  // namespace mamba

#endif  // MAMBA_SOLVER_HPP
