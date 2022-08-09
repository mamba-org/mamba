// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/solver_problems.hpp"

namespace mamba
{
    std::string MSolverProblem::to_string() const
    {
        return solver_problemruleinfo2str(
            solver, (SolverRuleinfo) type, source_id, target_id, dep_id);
    }

    std::optional<PackageInfo> MSolverProblem::target() const
    {
        if (target_id == 0 || target_id >= solver->pool->nsolvables)
            return std::nullopt;
        return pool_id2solvable(solver->pool, target_id);
    }

    std::optional<PackageInfo> MSolverProblem::source() const
    {
        if (source_id == 0 || source_id >= solver->pool->nsolvables)
            return std::nullopt;
        return pool_id2solvable(solver->pool, source_id);
    }

    std::optional<std::string> MSolverProblem::dep() const
    {
        if (!dep_id)
            return std::nullopt;
        return pool_dep2str(solver->pool, dep_id);
    }
}
