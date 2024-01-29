// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

#include <doctest/doctest.h>
#include <solv/pool.h>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/pool.hpp"

#include "doctest-printer/vector.hpp"

using namespace mamba::solv;

TEST_SUITE("solv::ObjPool")
{
    TEST_CASE("Construct a pool")
    {
        auto pool = ObjPool();

        SUBCASE("Change distribution type")
        {
            pool.set_disttype(DISTTYPE_CONDA);
            CHECK_EQ(pool.disttype(), DISTTYPE_CONDA);
        }

        SUBCASE("Add strings")
        {
            const auto id_hello = pool.add_string("Hello");
            const auto maybe_id_hello = pool.find_string("Hello");
            REQUIRE(maybe_id_hello.has_value());
            CHECK_EQ(maybe_id_hello.value(), id_hello);
            CHECK_EQ(pool.get_string(id_hello), "Hello");

            SUBCASE("Add another string")
            {
                const auto id_world = pool.add_string("World");
                CHECK_NE(id_world, id_hello);
                const auto maybe_id_world = pool.find_string("World");
                REQUIRE(maybe_id_world.has_value());
                CHECK_EQ(maybe_id_world.value(), id_world);
                CHECK_EQ(pool.get_string(id_world), "World");

                SUBCASE("Add the same one again")
                {
                    const auto id_world_again = pool.add_string("World");
                    CHECK_EQ(id_world_again, id_world);
                }
            }

            SUBCASE("Find non-existant string")
            {
                CHECK_FALSE(pool.find_string("Bar").has_value());
            }
        }

        SUBCASE("Add dependencies")
        {
            const auto id_name = pool.add_string("mamba");
            const auto id_version_1 = pool.add_string("1.0.0");

            const auto id_rel = pool.add_dependency(id_name, REL_GT, id_version_1);
            const auto maybe_id_rel = pool.find_dependency(id_name, REL_GT, id_version_1);
            REQUIRE(maybe_id_rel.has_value());
            CHECK_EQ(maybe_id_rel.value(), id_rel);
            CHECK_EQ(pool.get_dependency_name(id_rel), "mamba");
            CHECK_EQ(pool.get_dependency_relation(id_rel), " > ");
            CHECK_EQ(pool.get_dependency_version(id_rel), "1.0.0");
            CHECK_EQ(pool.dependency_to_string(id_rel), "mamba > 1.0.0");

            SUBCASE("Parse a conda dependency")
            {
                const auto id_conda = pool.add_conda_dependency("rattler < 0.1");
                CHECK_EQ(pool.get_dependency_name(id_conda), "rattler");
                CHECK_EQ(pool.get_dependency_version(id_conda), "<0.1");
            }
        }

        SUBCASE("Add repo")
        {
            auto [repo1_id, repo1] = pool.add_repo("repo1");
            CHECK_EQ(repo1.id(), repo1_id);
            REQUIRE(pool.has_repo(repo1_id));
            REQUIRE(pool.get_repo(repo1_id).has_value());
            CHECK_EQ(pool.get_repo(repo1_id).value().id(), repo1_id);
            CHECK_EQ(pool.repo_count(), 1);

            auto [repo2_id, repo2] = pool.add_repo("repo2");
            auto [repo3_id, repo3] = pool.add_repo("repo3");
            CHECK_EQ(pool.repo_count(), 3);

            SUBCASE("Add repo with same name")
            {
                auto [repo1_bis_id, repo1_bis] = pool.add_repo("repo1");
                CHECK_EQ(pool.repo_count(), 4);
                CHECK_NE(repo1_bis_id, repo1_id);
            }

            SUBCASE("Set installed repo")
            {
                CHECK_FALSE(pool.installed_repo().has_value());
                pool.set_installed_repo(repo2_id);
                REQUIRE(pool.installed_repo().has_value());
                CHECK_EQ(pool.installed_repo()->id(), repo2_id);
            }

            SUBCASE("Iterate over repos")
            {
                const auto repo_ids = std::array{ repo1_id, repo2_id, repo3_id };

                SUBCASE("Over all repos")
                {
                    std::size_t n_repos = 0;
                    pool.for_each_repo_id(
                        [&](RepoId id)
                        {
                            CHECK_NE(std::find(repo_ids.cbegin(), repo_ids.cend(), id), repo_ids.cend());
                            n_repos++;
                        }
                    );
                    CHECK_EQ(n_repos, pool.repo_count());
                }

                SUBCASE("Over one repo then break")
                {
                    std::size_t n_repos = 0;
                    pool.for_each_repo_id(
                        [&](RepoId)
                        {
                            n_repos++;
                            return LoopControl::Break;
                        }
                    );
                    CHECK_EQ(n_repos, 1);
                }
            }

            SUBCASE("Get inexisting repo")
            {
                CHECK_FALSE(pool.has_repo(1234));
                CHECK_FALSE(pool.get_repo(1234).has_value());
            }

            SUBCASE("Remove repo")
            {
                CHECK(pool.remove_repo(repo2_id, true));
                CHECK_FALSE(pool.has_repo(repo2_id));
                CHECK(pool.get_repo(repo1_id).has_value());
                CHECK_EQ(pool.repo_count(), 2);

                // Remove invalid repo is a noop
                CHECK_FALSE(pool.remove_repo(1234, true));
            }

            SUBCASE("Manage solvables")
            {
                auto [id1, s1] = repo1.add_solvable();
                const auto pkg_name_id = pool.add_string("mamba");
                const auto pkg_version_id = pool.add_string("1.0.0");
                s1.set_name(pkg_name_id);
                s1.set_version(pkg_version_id);
                s1.add_self_provide();

                auto [id2, s2] = repo2.add_solvable();
                s2.set_name(pkg_name_id);
                s2.set_version("2.0.0");
                s2.add_self_provide();

                SUBCASE("Retrieve solvables")
                {
                    CHECK_EQ(pool.solvable_count(), 2);
                    CHECK(pool.get_solvable(id1).has_value());
                    CHECK(pool.get_solvable(id2).has_value());
                }

                SUBCASE("Iterate over solvables")
                {
                    SUBCASE("Iterate over all solvables")
                    {
                        std::vector<SolvableId> ids = {};
                        pool.for_each_solvable_id([&](SolvableId id) { ids.push_back(id); });
                        std::sort(ids.begin(), ids.end());  // Ease comparison
                        CHECK_EQ(ids, decltype(ids){ id1, id2 });
                        pool.for_each_solvable(
                            [&](ObjSolvableViewConst s)
                            { CHECK_NE(std::find(ids.cbegin(), ids.cend(), s.id()), ids.cend()); }
                        );
                    }

                    SUBCASE("Over one solvable then break")
                    {
                        std::size_t n_solvables = 0;
                        pool.for_each_solvable_id(
                            [&](RepoId)
                            {
                                n_solvables++;
                                return LoopControl::Break;
                            }
                        );
                        CHECK_EQ(n_solvables, 1);
                    }
                }

                SUBCASE("Iterate on installed solvables")
                {
                    SUBCASE("No instaled repo")
                    {
                        pool.for_each_installed_solvable_id([&](SolvableId) { CHECK(false); });
                    }

                    SUBCASE("One installed repo")
                    {
                        pool.set_installed_repo(repo1_id);
                        std::vector<SolvableId> ids = {};
                        pool.for_each_installed_solvable_id([&](auto id) { ids.push_back(id); });
                        std::sort(ids.begin(), ids.end());  // Ease comparsion
                        CHECK_EQ(ids, decltype(ids){ id1 });
                    }
                }

                SUBCASE("Iterate through whatprovides")
                {
                    const auto dep_id = pool.add_dependency(pkg_name_id, REL_EQ, pkg_version_id);

                    SUBCASE("Without creating the whatprovides index is an error")
                    {
                        CHECK_THROWS_AS(
                            pool.for_each_whatprovides_id(dep_id, [&](auto) {}),
                            std::runtime_error
                        );
                    }

                    SUBCASE("With creation of whatprovides index")
                    {
                        pool.create_whatprovides();
                        auto whatprovides_ids = std::vector<SolvableId>();
                        pool.for_each_whatprovides_id(
                            dep_id,
                            [&](auto id) { whatprovides_ids.push_back(id); }
                        );
                        // Only one solvable matches
                        CHECK_EQ(whatprovides_ids, std::vector{ id1 });
                    }

                    SUBCASE("Namespace dependencies are not in whatprovies")
                    {
                        const auto other_dep_id = pool.add_dependency(
                            pkg_name_id,
                            REL_NAMESPACE,
                            pkg_version_id
                        );
                        pool.create_whatprovides();
                        bool called = false;
                        pool.for_each_whatprovides_id(other_dep_id, [&](auto) { called = true; });
                        CHECK_FALSE(called);
                    }

                    SUBCASE("Namespace names are in whatprovies")
                    {
                        pool.add_dependency(pkg_name_id, REL_NAMESPACE, pkg_version_id);
                        pool.create_whatprovides();
                        bool called = false;
                        // Diff below in other_dep_id > pkg_name_id
                        pool.for_each_whatprovides_id(pkg_name_id, [&](auto) { called = true; });
                        CHECK(called);
                    }
                }

                SUBCASE("Manually set whatprovides")
                {
                    const auto dep_id = pool.add_string("mydep");

                    SUBCASE("Without creating the whatprovides index is an error")
                    {
                        CHECK_THROWS_AS(
                            pool.add_to_whatprovides(dep_id, pool.add_to_whatprovides_data({ id1 })),
                            std::runtime_error
                        );
                    }

                    SUBCASE("With creation of whatprovides index")
                    {
                        pool.create_whatprovides();
                        pool.add_to_whatprovides(dep_id, pool.add_to_whatprovides_data({ id1 }));
                        auto whatprovides_ids = std::vector<SolvableId>();
                        pool.for_each_whatprovides_id(
                            dep_id,
                            [&](auto id) { whatprovides_ids.push_back(id); }
                        );
                        CHECK_EQ(whatprovides_ids, std::vector{ id1 });

                        SUBCASE("Gets cleared when calling create_whatprovides")
                        {
                            pool.create_whatprovides();
                            whatprovides_ids.clear();
                            pool.for_each_whatprovides_id(
                                dep_id,
                                [&](auto id) { whatprovides_ids.push_back(id); }
                            );
                            CHECK(whatprovides_ids.empty());
                        }
                    }
                }
            }
        }

        SUBCASE("Add a debug callback")
        {
            std::string_view message = "";
            int type = 0;
            pool.set_debug_callback(
                [&](auto* /* pool */, auto t, auto msg) noexcept
                {
                    message = msg;
                    type = t;
                }
            );
            pool_debug(pool.raw(), SOLV_DEBUG_RESULT, "Ho no!");
            CHECK_EQ(message, "Ho no!");
            CHECK_EQ(type, SOLV_DEBUG_RESULT);
        }

        SUBCASE("Add a namespace callback")
        {
            pool.set_namespace_callback(
                [&](auto* /* pool */, StringId /* name */, StringId /* version */) noexcept -> OffsetId
                { return 0; }
            );
        }
    }
}
