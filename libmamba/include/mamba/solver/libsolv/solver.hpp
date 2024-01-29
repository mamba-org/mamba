// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SOLVER_HPP
#define MAMBA_CORE_SOLVER_HPP

#include "mamba/core/error_handling.hpp"
#include "mamba/solver/libsolv/unsolvable.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/solver/solution.hpp"

namespace mamba
{

    class Solver
    {
    public:

        using Request = solver::Request;
        using Solution = solver::Solution;
        using UnSolvable = solver::libsolv::UnSolvable;
        using Outcome = std::variant<Solution, UnSolvable>;

        [[nodiscard]] auto solve(MPool& pool, const Request& request) -> expected_t<Outcome>;
    };
}
#endif  // MAMBA_SOLVER_HPP
