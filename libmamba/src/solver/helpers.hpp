// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_HELPERS_HPP
#define MAMBA_SOLVER_HELPERS_HPP

#include <functional>
#include <optional>

#include "mamba/solver/solution.hpp"
#include "mamba/specs/version.hpp"

/**
 * Solver, repo, and package helpers for solver agnostic code.
 */

namespace mamba::solver
{
    [[nodiscard]] auto find_new_python_in_solution(const Solution& solution
    ) -> std::optional<std::reference_wrapper<const specs::PackageInfo>>;

    [[nodiscard]] auto python_binary_compatible(  //
        const specs::Version& older,
        const specs::Version& newer
    ) -> bool;
}
#endif
