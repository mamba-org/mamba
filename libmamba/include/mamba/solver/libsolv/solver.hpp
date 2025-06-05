// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_LIBSOLV_SOLVER_HPP
#define MAMBA_SOLVER_LIBSOLV_SOLVER_HPP

#include "mamba/core/error_handling.hpp"
#include "mamba/solver/libsolv/parameters.hpp"
#include "mamba/solver/libsolv/unsolvable.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/solver/solution.hpp"

namespace mamba::solver::libsolv
{
    class Database;

    class Solver
    {
    public:

        using Outcome = std::variant<Solution, UnSolvable>;

        [[nodiscard]] auto
        solve(Database& database, Request&& request, MatchSpecParser ms_parser = MatchSpecParser::Mixed)
            -> expected_t<Outcome>;

        [[nodiscard]] auto
        solve(Database& database, const Request& request, MatchSpecParser ms_parser = MatchSpecParser::Mixed)
            -> expected_t<Outcome>;

    private:

        auto solve_impl(Database& database, const Request& request, MatchSpecParser ms_parser)
            -> expected_t<Outcome>;
    };
}
#endif
