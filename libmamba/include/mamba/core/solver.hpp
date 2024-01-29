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

#include "mamba/core/error_handling.hpp"
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

    class UnSolvable
    {
    public:

        UnSolvable(UnSolvable&&);

        ~UnSolvable();

        auto operator=(UnSolvable&&) -> UnSolvable&;

        [[nodiscard]] auto problems_to_str(MPool& pool) const -> std::string;
        [[nodiscard]] auto all_problems(MPool& pool) const -> std::vector<std::string>;
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

        [[nodiscard]] auto solve(MPool& pool, const Request& request) -> expected_t<Outcome>;
    };
}  // namespace mamba

#endif  // MAMBA_SOLVER_HPP
