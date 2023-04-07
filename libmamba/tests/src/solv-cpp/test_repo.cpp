// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>

#include <doctest/doctest.h>

#include "mamba/core/util.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"

using namespace mamba::solv;

TEST_SUITE("ObjRepo")
{
    TEST_CASE("Construct a repo")
    {
        auto pool = ObjPool();
        auto repo_id = pool.add_repo("test-forge");
        auto repo = pool.get_repo(repo_id);

        SUBCASE("Fetch the repo")
        {
            CHECK_EQ(pool.get_repo(repo_id).name(), "test-forge");
        }

        SUBCASE("Set attributes")
        {
            repo.set_url("https://repo.mamba.pm/conda-forge");
            repo.set_channel("conda-forge");
            repo.set_subdir("noarch");

            SUBCASE("Empty without internalize")
            {
                CHECK_EQ(repo.url(), "");
                CHECK_EQ(repo.channel(), "");
                CHECK_EQ(repo.subdir(), "");
            }

            SUBCASE("Internalize and get attributes")
            {
                repo.internalize();

                CHECK_EQ(repo.url(), "https://repo.mamba.pm/conda-forge");
                CHECK_EQ(repo.channel(), "conda-forge");
                CHECK_EQ(repo.subdir(), "noarch");

                SUBCASE("Override attribute")
                {
                    repo.set_subdir("linux-64");
                    CHECK_EQ(repo.subdir(), "noarch");
                    repo.internalize();
                    CHECK_EQ(repo.subdir(), "linux-64");
                }
            }
        }

        SUBCASE("Add solvables")
        {
            // Adding solvable
            CHECK_EQ(repo.n_solvables(), 0);
            const auto id1 = repo.add_solvable();
            CHECK_EQ(repo.n_solvables(), 1);
            CHECK(repo.contains_solvable(id1));
            const auto id2 = repo.add_solvable();
            CHECK_EQ(repo.n_solvables(), 2);
            CHECK(repo.contains_solvable(id2));

            SUBCASE("Iterate through solvables")
            {
                // Iterating through solvables
                const auto ids = std::array{ id1, id2 };
                std::size_t n_solvables = 0;
                repo.for_each_solvable_id(
                    [&](SolvableId id)
                    {
                        CHECK_NE(std::find(ids.cbegin(), ids.cend(), id), ids.cend());
                        n_solvables++;
                    }
                );
                CHECK_EQ(n_solvables, repo.n_solvables());
            }

            SUBCASE("Clear solvables")
            {
                repo.clear(true);
                CHECK_EQ(repo.n_solvables(), 0);
            }

            SUBCASE("Write repo to file")
            {
                auto dir = mamba::TemporaryDirectory();
                const auto solv_file = dir.path() / "test-forge.hpp";
                repo.write(solv_file);

                SUBCASE("Read repo from file")
                {
                    // Delete repo
                    const auto n_solvables = repo.n_solvables();
                    pool.remove_repo(repo_id, true);

                    // Create new repo from file
                    auto repo_id2 = pool.add_repo("test-forge");
                    auto repo2 = pool.get_repo(repo_id2);
                    repo2.read(solv_file);

                    CHECK_EQ(repo2.n_solvables(), n_solvables);
                    // True because we reused ids
                    CHECK(repo2.contains_solvable(id1));
                    CHECK(repo2.contains_solvable(id2));
                }
            }
        }
    }
}
