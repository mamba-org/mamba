// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>
#include <solv/solver.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/solver.hpp"

using namespace mamba::solv;

TEST_SUITE("ObjSolver")
{
    TEST_CASE("Create a solver")
    {
        auto pool = ObjPool();
        auto solver = ObjSolver(pool);

        CHECK_EQ(solver.problem_count(), 0);

        SUBCASE("Flag default value")
        {
            CHECK_FALSE(solver.get_flag(SOLVER_FLAG_ALLOW_DOWNGRADE));
        }

        SUBCASE("Set flag")
        {
            solver.set_flag(SOLVER_FLAG_ALLOW_DOWNGRADE, true);
            CHECK(solver.get_flag(SOLVER_FLAG_ALLOW_DOWNGRADE));
        }
    }
}
