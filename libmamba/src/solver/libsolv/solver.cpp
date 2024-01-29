// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <solv/solver.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "solv-cpp/solver.hpp"

#include "solver/libsolv/helpers.hpp"

namespace mamba
{
    namespace
    {
        void set_solver_flags(solv::ObjSolver& solver, const solver::Request::Flags& flags)
        {
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_ALLOW_DOWNGRADE, flags.allow_downgrade);
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_ALLOW_UNINSTALL, flags.allow_uninstall);
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_STRICT_REPO_PRIORITY, flags.strict_repo_priority);
        }
    }

    auto Solver::solve(MPool& mpool, const Request& request) -> expected_t<Outcome>
    {
        auto& pool = mpool.pool();
        const auto& chan_params = mpool.channel_context().params();
        const auto& flags = request.flags;

        return solver::libsolv::request_to_decision_queue(request, pool, chan_params, flags.force_reinstall)
            .transform(
                [&](auto&& jobs) -> Outcome
                {
                    auto solver = std::make_unique<solv::ObjSolver>(pool);
                    set_solver_flags(*solver, flags);
                    const bool success = solver->solve(pool, jobs);
                    if (!success)
                    {
                        return { UnSolvable(std::move(solver)) };
                    }

                    auto trans = solv::ObjTransaction::from_solver(pool, *solver);
                    trans.order(pool);

                    if (!flags.keep_user_specs && flags.keep_dependencies)
                    {
                        return {
                            solver::libsolv::transaction_to_solution_only_deps(pool, trans, request)
                        };
                    }
                    else if (flags.keep_user_specs && !flags.keep_dependencies)
                    {
                        return {
                            solver::libsolv::transaction_to_solution_no_deps(pool, trans, request)
                        };
                    }
                    return { solver::libsolv::transaction_to_solution(pool, trans) };
                }
            );
    }

}  // namespace mamba
