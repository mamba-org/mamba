// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>

#include <doctest/doctest.h>

#include "mamba/core/util.hpp"
#include "mamba/util/cfile.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"

using namespace mamba::solv;
using namespace mamba;

TEST_SUITE("solv::ObjRepo")
{
    TEST_CASE("Construct a repo")
    {
        auto pool = ObjPool();
        auto [repo_id, repo] = pool.add_repo("test-forge");
        CHECK_EQ(repo.id(), repo_id);
        CHECK_EQ(repo.name(), "test-forge");

        SUBCASE("Fetch the repo")
        {
            REQUIRE(pool.has_repo(repo_id));
            auto repo_alt = pool.get_repo(repo_id).value();
            CHECK_EQ(repo_alt.name(), repo.name());
            CHECK_EQ(repo_alt.id(), repo.id());
        }

        SUBCASE("Set attributes")
        {
            repo.set_url("https://repo.mamba.pm/conda-forge");
            repo.set_etag(R"(etag)W/"8eea3023872b68ef71fd930472a15599"(etag)");
            repo.set_mod("Tue, 25 Apr 2023 11:48:37 GMT");
            repo.set_channel("conda-forge");
            repo.set_subdir("noarch");
            repo.set_pip_added(true);
            repo.set_tool_version("1.2.3.4");

            SUBCASE("Empty without internalize")
            {
                CHECK_EQ(repo.url(), "");
                CHECK_EQ(repo.etag(), "");
                CHECK_EQ(repo.mod(), "");
                CHECK_EQ(repo.channel(), "");
                CHECK_EQ(repo.subdir(), "");
                CHECK_EQ(repo.pip_added(), false);
                CHECK_EQ(repo.tool_version(), "");
            }

            SUBCASE("Internalize and get attributes")
            {
                repo.internalize();

                CHECK_EQ(repo.url(), "https://repo.mamba.pm/conda-forge");
                CHECK_EQ(repo.channel(), "conda-forge");
                CHECK_EQ(repo.subdir(), "noarch");
                CHECK_EQ(repo.etag(), R"(etag)W/"8eea3023872b68ef71fd930472a15599"(etag)");
                CHECK_EQ(repo.mod(), "Tue, 25 Apr 2023 11:48:37 GMT");
                CHECK_EQ(repo.pip_added(), true);
                CHECK_EQ(repo.tool_version(), "1.2.3.4");

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
            CHECK_EQ(repo.solvable_count(), 0);
            const auto [id1, s1] = repo.add_solvable();
            REQUIRE(repo.get_solvable(id1).has_value());
            CHECK_EQ(repo.get_solvable(id1)->raw(), s1.raw());
            CHECK_EQ(repo.solvable_count(), 1);
            CHECK(repo.has_solvable(id1));
            const auto [id2, s2] = repo.add_solvable();
            CHECK_EQ(repo.solvable_count(), 2);
            CHECK(repo.has_solvable(id2));

            SUBCASE("Retrieve repo from solvable")
            {
                CHECK_EQ(ObjRepoViewConst::of_solvable(s1).raw(), repo.raw());
            }

            SUBCASE("Iterate over solvables")
            {
                SUBCASE("Over all solvables")
                {
                    const auto ids = std::array{ id1, id2 };
                    std::size_t n_solvables = 0;
                    repo.for_each_solvable_id(
                        [&](SolvableId id)
                        {
                            CHECK_NE(std::find(ids.cbegin(), ids.cend(), id), ids.cend());
                            n_solvables++;
                        }
                    );
                    CHECK_EQ(n_solvables, repo.solvable_count());
                }

                SUBCASE("Over one solvable then break")
                {
                    std::size_t n_solvables = 0;
                    repo.for_each_solvable(
                        [&](ObjSolvableView)
                        {
                            n_solvables++;
                            return LoopControl::Break;
                        }
                    );
                    CHECK_EQ(n_solvables, 1);
                }
            }

            SUBCASE("Get inexisting solvable")
            {
                CHECK_FALSE(repo.has_solvable(1234));
                CHECK_FALSE(repo.get_solvable(1234).has_value());
            }

            SUBCASE("Remove solvable")
            {
                CHECK(repo.remove_solvable(id2, true));
                CHECK_FALSE(repo.has_solvable(id2));
                CHECK(repo.has_solvable(id1));
                CHECK_EQ(repo.solvable_count(), 1);
            }

            SUBCASE("Confuse ids from another repo")
            {
                auto [other_repo_id, other_repo] = pool.add_repo("other-repo");
                auto [other_id, other_s] = other_repo.add_solvable();

                CHECK_FALSE(repo.has_solvable(other_id));
                CHECK_FALSE(repo.get_solvable(other_id).has_value());
                CHECK_FALSE(repo.remove_solvable(other_id, true));
            }

            SUBCASE("Clear solvables")
            {
                repo.clear(true);
                CHECK_EQ(repo.solvable_count(), 0);
                CHECK_FALSE(repo.has_solvable(id1));
                CHECK_FALSE(repo.get_solvable(id1).has_value());
            }

            SUBCASE("Write repo to file")
            {
                auto dir = mamba::TemporaryDirectory();
                const auto solv_file = dir.path() / "test-forge.hpp";
                {
                    auto fptr = util::CFile::try_open(solv_file, "wb");
                    REQUIRE(fptr);
                    const auto written = repo.write(fptr->raw());
                    REQUIRE(written);
                    REQUIRE(fptr->try_close());
                }

                SUBCASE("Read repo from file")
                {
                    // Delete repo
                    const auto n_solvables = repo.solvable_count();
                    pool.remove_repo(repo_id, true);

                    // Create new repo from file
                    auto [repo_id2, repo2] = pool.add_repo("test-forge");
                    auto fptr = util::CFile::try_open(solv_file, "rb");
                    REQUIRE(fptr);
                    const auto read = repo2.read(fptr->raw());
                    REQUIRE(read);
                    REQUIRE(fptr->try_close());

                    CHECK_EQ(repo2.solvable_count(), n_solvables);
                    // True because we reused ids
                    CHECK(repo2.has_solvable(id1));
                    CHECK(repo2.has_solvable(id2));
                }
            }
        }
    }
}
