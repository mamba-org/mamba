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

    inline std::string MSolverProblem::to_string() const
    {
        return solver_problemruleinfo2str(
            solver, (SolverRuleinfo) type, source_id, target_id, dep_id);
    }

    inline std::optional<PackageInfo> MSolverProblem::target() const
    {
        if (target_id == 0 || target_id >= solver->pool->nsolvables)
            return std::nullopt;
        return pool_id2solvable(solver->pool, target_id);
    }

    inline std::optional<PackageInfo> MSolverProblem::source() const
    {
        if (source_id == 0 || source_id >= solver->pool->nsolvables)
            return std::nullopt;
        ;
        return pool_id2solvable(solver->pool, source_id);
    }

    inline std::optional<std::string> MSolverProblem::dep() const
    {
        if (!dep_id)
            return std::nullopt;
        return pool_dep2str(solver->pool, dep_id);
    }

}  // namespace mamba

#endif  // MAMBA_SOLVER_PROBLEMS_HPP
