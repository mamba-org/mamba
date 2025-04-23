// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>
#include <solv/solver.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/solver.hpp"

#include "pool_data.hpp"

using namespace solv;
using namespace solv::test;

namespace
{
    TEST_CASE("Create a solver")
    {
        auto pool = ObjPool();
        auto [repo_id, repo] = pool.add_repo("forge");
        add_simple_packages(pool, repo, make_packages());
        repo.internalize();

        auto solver = ObjSolver(pool);

        REQUIRE(solver.problem_count() == 0);

        SECTION("Flag default value")
        {
            REQUIRE_FALSE(solver.get_flag(SOLVER_FLAG_ALLOW_DOWNGRADE));
        }

        SECTION("Set flag")
        {
            solver.set_flag(SOLVER_FLAG_ALLOW_DOWNGRADE, true);
            REQUIRE(solver.get_flag(SOLVER_FLAG_ALLOW_DOWNGRADE));
        }

        SECTION("Solve successfully")
        {
            // The job is matched with the ``provides`` field of the solvable
            auto jobs = ObjQueue{
                SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                pool.add_legacy_conda_dependency("menu"),
                SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                pool.add_legacy_conda_dependency("icons=2.*"),
            };
            REQUIRE(solver.solve(pool, jobs));
            REQUIRE(solver.problem_count() == 0);
        }

        SECTION("Solve unsuccessfully with conflict")
        {
            // The job is matched with the ``provides`` field of the solvable
            auto jobs = ObjQueue{
                SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                pool.add_legacy_conda_dependency("menu"),
                SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                pool.add_legacy_conda_dependency("icons=1.*"),
                SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                pool.add_legacy_conda_dependency("intl=5.*"),
            };

            REQUIRE_FALSE(solver.solve(pool, jobs));
            REQUIRE(solver.problem_count() != 0);

            auto all_rules = ObjQueue{};
            solver.for_each_problem_id(
                [&](auto pb)
                {
                    auto pb_rules = solver.problem_rules(pb);
                    all_rules.insert(all_rules.end(), pb_rules.cbegin(), pb_rules.cend());
                }
            );
            REQUIRE_FALSE(all_rules.empty());
        }

        SECTION("Solve unsuccessfully with missing package")
        {
            // The job is matched with the ``provides`` field of the solvable
            auto jobs = ObjQueue{
                SOLVER_INSTALL | SOLVER_SOLVABLE_PROVIDES,
                pool.add_legacy_conda_dependency("does-not-exists"),
            };

            REQUIRE_FALSE(solver.solve(pool, jobs));
            REQUIRE(solver.problem_count() != 0);

            auto all_rules = ObjQueue{};
            solver.for_each_problem_id(
                [&](auto pb)
                {
                    auto pb_rules = solver.problem_rules(pb);
                    all_rules.insert(all_rules.end(), pb_rules.cbegin(), pb_rules.cend());
                }
            );
            REQUIRE_FALSE(all_rules.empty());
        }
    }
}
