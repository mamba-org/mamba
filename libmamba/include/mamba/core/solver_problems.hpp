// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_PROBLEMS_HPP
#define MAMBA_SOLVER_PROBLEMS_HPP

#include <string>
#include <utility>
#include <vector>
#include <optional>

#include "match_spec.hpp"
#include "package_info.hpp"

extern "C"
{
#include "solv/solver.h"
}

namespace mamba
{
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

}  // namespace mamba

#endif  // MAMBA_SOLVER_PROBLEMS_HPP
