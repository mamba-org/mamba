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
#include "mamba/util/variant_cmp.hpp"
#include "solv-cpp/solver.hpp"

#include "solver/libsolv/helpers.hpp"

namespace mamba::solver::libsolv
{
    namespace
    {
        void set_solver_flags(solv::ObjSolver& solver, const solver::Request::Flags& flags)
        {
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_ALLOW_DOWNGRADE, flags.allow_downgrade);
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_ALLOW_UNINSTALL, flags.allow_uninstall);
            ::solver_set_flag(solver.raw(), SOLVER_FLAG_STRICT_REPO_PRIORITY, flags.strict_repo_priority);
        }

        /**
         * An arbitrary comparison function to get determinist output.
         *
         * Could be improved as libsolv seems to be sensitive to sort order.
         * https://github.com/mamba-org/mamba/issues/3058
         */
        auto make_request_cmp()
        {
            return util::make_variant_cmp(
                /** index_cmp= */
                [](auto lhs, auto rhs) { return lhs < rhs; },
                /** alternative_cmp= */
                [](const auto& lhs, const auto& rhs)
                {
                    using Itm = std::decay_t<decltype(lhs)>;
                    if constexpr (!std::is_same_v<Itm, Request::UpdateAll>)
                    {
                        return lhs.spec.name().str() < rhs.spec.name().str();
                    }
                    return false;
                }
            );
        }
    }

    auto Solver::solve_impl(MPool& mpool, const Request& request) -> expected_t<Outcome>
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

                    return { solver::libsolv::transaction_to_solution(pool, trans, request, flags) };
                }
            );
    }

    auto Solver::solve(MPool& mpool, Request&& request) -> expected_t<Outcome>
    {
        if (request.flags.order_request)
        {
            std::sort(request.items.begin(), request.items.end(), make_request_cmp());
        }
        return solve_impl(mpool, request);
    }

    auto Solver::solve(MPool& mpool, const Request& request) -> expected_t<Outcome>
    {
        if (request.flags.order_request)
        {
            auto sorted_request = request;
            std::sort(sorted_request.items.begin(), sorted_request.items.end(), make_request_cmp());
            return solve_impl(mpool, sorted_request);
        }
        return solve_impl(mpool, request);
    }

}  // namespace mamba
