// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <solv/solver.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"
#include "solv-cpp/solver.hpp"

#include "pool-data.hpp"

using namespace mamba::solv;

TEST_SUITE("ObjSolver")
{
    TEST_CASE("Create a solver")
    {
        auto pool = ObjPool();
        auto [repo_id, repo] = pool.add_repo("forge");
        mamba::test::add_simple_packages(pool, repo);
        repo.internalize();

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

        SUBCASE("Add packages")
        {
            SUBCASE("Solve successfully")
            {
                // The job is matched with the ``provides`` field of the solvable
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("menu"),
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("icons=2.*"),
                };
                CHECK(solver.solve(pool, jobs));
                CHECK_EQ(solver.problem_count(), 0);
            }

            SUBCASE("Solve unsuccessfully")
            {
                // The job is matched with the ``provides`` field of the solvable
                auto jobs = ObjQueue{
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("menu"),
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("icons=1.*"),
                    SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                    pool.add_conda_dependency("intl=5.*"),
                };

                CHECK_FALSE(solver.solve(pool, jobs));
                CHECK_NE(solver.problem_count(), 0);

                auto all_rules = ObjQueue{};
                solver.for_each_problem_id(
                    [&](auto pb)
                    {
                        auto pb_rules = solver.problem_rules(pb);
                        all_rules.insert(all_rules.end(), pb_rules.cbegin(), pb_rules.cend());
                    }
                );
                CHECK_FALSE(all_rules.empty());
            }
        }
    }
}
