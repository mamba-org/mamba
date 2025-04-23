// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>

#include <catch2/catch_all.hpp>
#include <solv/solver.h>
#include <solv/transaction.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/solver.hpp"
#include "solv-cpp/transaction.hpp"

#include "pool_data.hpp"

using namespace solv;
using namespace solv::test;

namespace
{
    TEST_CASE("Create a transaction")
    {
        auto pool = ObjPool();
        auto [repo_id, repo] = pool.add_repo("forge");
        const auto pkg_to_id = add_simple_packages(pool, repo, make_packages());
        repo.internalize();

        SECTION("From single packages")
        {
            SECTION("Add not installed package")
            {
                pool.create_whatprovides();
                const auto id = pkg_to_id.at({ "menu", "1.5.0", { "dropdown=2.*" } });
                auto trans = ObjTransaction::from_solvables(pool, { id });
                REQUIRE(trans.steps() == ObjQueue{ id });
                REQUIRE(trans.step_type(pool, id) == SOLVER_TRANSACTION_INSTALL);
            }

            SECTION("Ignore removing not installed package")
            {
                pool.create_whatprovides();
                const auto id = pkg_to_id.at({ "menu", "1.5.0", { "dropdown=2.*" } });
                // Negative id means remove
                auto trans = ObjTransaction::from_solvables(pool, { -id });
                REQUIRE(trans.empty());
                REQUIRE(trans.step_type(pool, id) == SOLVER_TRANSACTION_IGNORE);
            }

            SECTION("Ignore adding installed package")
            {
                const auto id = pkg_to_id.at({ "menu", "1.5.0", { "dropdown=2.*" } });
                pool.set_installed_repo(repo_id);
                pool.create_whatprovides();
                auto trans = ObjTransaction::from_solvables(pool, { id });
                REQUIRE(trans.empty());
                REQUIRE(trans.step_type(pool, id) == SOLVER_TRANSACTION_IGNORE);
            }

            SECTION("Remove installed package")
            {
                const auto id = pkg_to_id.at({ "menu", "1.5.0", { "dropdown=2.*" } });
                pool.set_installed_repo(repo_id);
                pool.create_whatprovides();
                // Negative id means remove
                auto trans = ObjTransaction::from_solvables(pool, { -id });
                REQUIRE(trans.steps() == ObjQueue{ id });
                REQUIRE(trans.step_type(pool, id) == SOLVER_TRANSACTION_ERASE);
            }
        }

        SECTION("From a list of package to install")
        {
            pool.create_whatprovides();
            const auto solvables = ObjQueue{
                pkg_to_id.at({ "menu", "1.5.0", { "dropdown=2.*" } }),
                pkg_to_id.at({ "dropdown", "2.3.0", { "icons=2.*" } }),
                pkg_to_id.at({ "icons", "2.0.0" }),
            };
            auto trans = ObjTransaction::from_solvables(pool, solvables);

            REQUIRE_FALSE(trans.empty());
            REQUIRE(trans.size() == solvables.size());
            REQUIRE(trans.steps() == solvables);

            SECTION("Copy transaction")
            {
                const auto copy = trans;
                REQUIRE(copy.steps() == solvables);
            }

            SECTION("Order the solvables")
            {
                trans.order(pool);
                REQUIRE(trans.steps() == ObjQueue{ solvables.crbegin(), solvables.crend() });
            }
        }

        SECTION("From a solver run")
        {
            auto [installed_id, installed] = pool.add_repo("installed");
            const auto icons_id = add_simple_package(pool, installed, { "icons", "1.0.0" });
            installed.internalize();
            pool.set_installed_repo(installed_id);
            pool.create_whatprovides();

            auto solver = ObjSolver(pool);
            REQUIRE(solver.solve(pool, { SOLVER_INSTALL, pool.add_legacy_conda_dependency("menu>=1.4") }));
            auto trans = ObjTransaction::from_solver(pool, solver);
            REQUIRE_FALSE(trans.empty());
            REQUIRE(trans.size() == 4);

            SECTION("Outdated installed package is updated")
            {
                REQUIRE(trans.steps().contains(icons_id));
                REQUIRE(trans.step_type(pool, icons_id) == SOLVER_TRANSACTION_UPGRADED);
                // The solvable id that upgrades ``icons_id``
                const auto maybe_icons_update_id = trans.step_newer(pool, icons_id);
                REQUIRE(maybe_icons_update_id.has_value());
                REQUIRE(trans.steps().contains(maybe_icons_update_id.value()));
                // The solvable id that isupgraded by ``icons_id``
                REQUIRE(trans.step_olders(pool, maybe_icons_update_id.value()) == ObjQueue{ icons_id });
            }

            SECTION("Classify the transaction elements")
            {
                auto solvables = ObjQueue();

                trans.classify_for_each_type(
                    pool,
                    [&](TransactionStepType /*type*/, const ObjQueue& ids)
                    {
                        for (const SolvableId id : ids)
                        {
                            // Adding ids found in the classification
                            solvables.push_back(id);
                            // Adding as well the newer update of those solvables if there is one
                            if (auto maybe_new = trans.step_newer(pool, id); maybe_new.has_value())
                            {
                                solvables.push_back(maybe_new.value());
                            }
                        }
                    }
                );

                // Sorting for comparison
                std::sort(solvables.begin(), solvables.end());
                auto steps = trans.steps();
                std::sort(steps.begin(), steps.end());
                REQUIRE(solvables == steps);
            }
        }
    }
}
