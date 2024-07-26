#pragma once

#include "mamba/core/error_handling.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/solver/resolvo/database.hpp"
#include "mamba/solver/resolvo/unsolvable.hpp"
#include "mamba/solver/solution.hpp"
#include "mamba/util/variant_cmp.hpp"

namespace mamba::solver::resolvo
{

    class Solver
    {
    public:

        using Outcome = std::variant<Solution, UnSolvable>;

        [[nodiscard]] auto solve(PackageDatabase& pool, Request&& request) -> expected_t<Outcome>;
        [[nodiscard]] auto
        solve(PackageDatabase& pool, const Request& request) -> expected_t<Outcome>;

    private:

        auto solve_impl(PackageDatabase& pool, const Request& request) -> expected_t<Outcome>;
    };
}
