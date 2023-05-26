// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/transaction.hpp"

#include "pool-data.hpp"

using namespace mamba::solv;

TEST_SUITE("ObjTransaction")
{
    TEST_CASE("Create a transaction")
    {
        auto pool = ObjPool();
        auto [repo_id, repo] = pool.add_repo("forge");
        mamba::test::add_simple_packages(pool, repo);
        repo.internalize();
        pool.create_whatprovides();

        SUBCASE("From a list of package to install")
        {
            const ObjQueue solvables = [&]()
            {
                ObjQueue solvables = {};
                pool.for_each_solvable_id([&](auto id) { solvables.push_back(id); });
                return solvables;
            }();

            // Install all solvables
            auto trans = ObjTransaction::from_solvables(pool, solvables);

            const ObjQueue steps = [&]()
            {
                ObjQueue steps = {};
                trans.for_each_step_id([&](auto id) { steps.push_back(id); });
                return steps;
            }();

            CHECK_EQ(steps, solvables);
        }
    }
}
