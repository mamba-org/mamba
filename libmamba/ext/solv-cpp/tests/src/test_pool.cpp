// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>
#include <solv/pool.h>
#include <solv/solver.h>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/pool.hpp"

using namespace solv;

TEST_CASE("Construct a pool")
{
    auto pool = ObjPool();

    SECTION("Change distribution type")
    {
        pool.set_disttype(DISTTYPE_CONDA);
        REQUIRE(pool.disttype() == DISTTYPE_CONDA);
    }

    SECTION("Error")
    {
        pool.set_current_error("Some failure");
        REQUIRE(pool.current_error() == "Some failure");
    }

    SECTION("Add strings")
    {
        const auto id_hello = pool.add_string("Hello");
        const auto maybe_id_hello = pool.find_string("Hello");
        REQUIRE(maybe_id_hello.has_value());
        REQUIRE(maybe_id_hello.value() == id_hello);
        REQUIRE(pool.get_string(id_hello) == "Hello");

        SECTION("Add another string")
        {
            const auto id_world = pool.add_string("World");
            REQUIRE(id_world != id_hello);
            const auto maybe_id_world = pool.find_string("World");
            REQUIRE(maybe_id_world.has_value());
            REQUIRE(maybe_id_world.value() == id_world);
            REQUIRE(pool.get_string(id_world) == "World");

            SECTION("Add the same one again")
            {
                const auto id_world_again = pool.add_string("World");
                REQUIRE(id_world_again == id_world);
            }
        }

        SECTION("Find non-existent string")
        {
            REQUIRE_FALSE(pool.find_string("Bar").has_value());
        }
    }

    SECTION("Add dependencies")
    {
        const auto id_name = pool.add_string("mamba");
        const auto id_version_1 = pool.add_string("1.0.0");

        const auto id_rel = pool.add_dependency(id_name, REL_GT, id_version_1);
        const auto maybe_id_rel = pool.find_dependency(id_name, REL_GT, id_version_1);
        REQUIRE(maybe_id_rel.has_value());
        REQUIRE(maybe_id_rel.value() == id_rel);
        REQUIRE(pool.get_dependency_name(id_rel) == "mamba");
        REQUIRE(pool.get_dependency_relation(id_rel) == " > ");
        REQUIRE(pool.get_dependency_version(id_rel) == "1.0.0");
        REQUIRE(pool.dependency_to_string(id_rel) == "mamba > 1.0.0");

        SECTION("Parse a conda dependency")
        {
            const auto id_conda = pool.add_conda_dependency("rattler < 0.1");
            REQUIRE(pool.get_dependency_name(id_conda) == "rattler");
            REQUIRE(pool.get_dependency_version(id_conda) == "<0.1");
        }
    }

    SECTION("Add repo")
    {
        auto [repo1_id, repo1] = pool.add_repo("repo1");
        REQUIRE(repo1.id() == repo1_id);
        REQUIRE(pool.has_repo(repo1_id));
        REQUIRE(pool.get_repo(repo1_id).has_value());
        REQUIRE(pool.get_repo(repo1_id).value().id() == repo1_id);
        REQUIRE(pool.repo_count() == 1);

        auto [repo2_id, repo2] = pool.add_repo("repo2");
        auto [repo3_id, repo3] = pool.add_repo("repo3");
        REQUIRE(pool.repo_count() == 3);

        SECTION("Add repo with same name")
        {
            auto [repo1_bis_id, repo1_bis] = pool.add_repo("repo1");
            REQUIRE(pool.repo_count() == 4);
            REQUIRE(repo1_bis_id != repo1_id);
        }

        SECTION("Set installed repo")
        {
            REQUIRE_FALSE(pool.installed_repo().has_value());
            pool.set_installed_repo(repo2_id);
            REQUIRE(pool.installed_repo().has_value());
            REQUIRE(pool.installed_repo()->id() == repo2_id);
        }

        SECTION("Iterate over repos")
        {
            const auto repo_ids = std::array{ repo1_id, repo2_id, repo3_id };

            SECTION("Over all repos")
            {
                std::size_t n_repos = 0;
                pool.for_each_repo_id(
                    [&](RepoId id)
                    {
                        REQUIRE(std::find(repo_ids.cbegin(), repo_ids.cend(), id) != repo_ids.cend());
                        n_repos++;
                    }
                );
                REQUIRE(n_repos == pool.repo_count());
            }

            SECTION("Over one repo then break")
            {
                std::size_t n_repos = 0;
                pool.for_each_repo_id(
                    [&](RepoId)
                    {
                        n_repos++;
                        return LoopControl::Break;
                    }
                );
                REQUIRE(n_repos == 1);
            }
        }

        SECTION("Get inexisting repo")
        {
            REQUIRE_FALSE(pool.has_repo(1234));
            REQUIRE_FALSE(pool.get_repo(1234).has_value());
        }

        SECTION("Remove repo")
        {
            REQUIRE(pool.remove_repo(repo2_id, true));
            REQUIRE_FALSE(pool.has_repo(repo2_id));
            REQUIRE(pool.get_repo(repo1_id).has_value());
            REQUIRE(pool.repo_count() == 2);

            // Remove invalid repo is a noop
            REQUIRE_FALSE(pool.remove_repo(1234, true));
        }

        SECTION("Manage solvables")
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

            SECTION("Retrieve solvables")
            {
                REQUIRE(pool.solvable_count() == 2);
                REQUIRE(pool.get_solvable(id1).has_value());
                REQUIRE(pool.get_solvable(id2).has_value());
            }

            SECTION("Iterate over solvables")
            {
                SECTION("Iterate over all solvables")
                {
                    std::vector<SolvableId> ids = {};
                    pool.for_each_solvable_id([&](SolvableId id) { ids.push_back(id); });
                    std::sort(ids.begin(), ids.end());  // Ease comparison
                    REQUIRE(ids == decltype(ids){ id1, id2 });
                    pool.for_each_solvable(
                        [&](ObjSolvableViewConst s)
                        { REQUIRE(std::find(ids.cbegin(), ids.cend(), s.id()) != ids.cend()); }
                    );
                }

                SECTION("Over one solvable then break")
                {
                    std::size_t n_solvables = 0;
                    pool.for_each_solvable_id(
                        [&](RepoId)
                        {
                            n_solvables++;
                            return LoopControl::Break;
                        }
                    );
                    REQUIRE(n_solvables == 1);
                }
            }

            SECTION("Iterate on installed solvables")
            {
                SECTION("No installed repo")
                {
                    pool.for_each_installed_solvable_id([&](SolvableId) { REQUIRE(false); });
                }

                SECTION("One installed repo")
                {
                    pool.set_installed_repo(repo1_id);
                    std::vector<SolvableId> ids = {};
                    pool.for_each_installed_solvable_id([&](auto id) { ids.push_back(id); });
                    std::sort(ids.begin(), ids.end());  // Ease comparison
                    REQUIRE(ids == decltype(ids){ id1 });
                }
            }

            SECTION("Iterate through whatprovides")
            {
                const auto dep_id = pool.add_dependency(pkg_name_id, REL_EQ, pkg_version_id);

                SECTION("Without creating the whatprovides index is an error")
                {
                    REQUIRE_THROWS_AS(
                        pool.for_each_whatprovides_id(dep_id, [&](auto) {}),
                        std::runtime_error
                    );
                }

                SECTION("With creation of whatprovides index")
                {
                    pool.create_whatprovides();
                    auto whatprovides_ids = std::vector<SolvableId>();
                    pool.for_each_whatprovides_id(
                        dep_id,
                        [&](auto id) { whatprovides_ids.push_back(id); }
                    );
                    // Only one solvable matches
                    REQUIRE(whatprovides_ids == std::vector{ id1 });
                }

                SECTION("Namespace dependencies are not in whatprovies")
                {
                    const auto other_dep_id = pool.add_dependency(
                        pkg_name_id,
                        REL_NAMESPACE,
                        pkg_version_id
                    );
                    pool.create_whatprovides();
                    bool called = false;
                    pool.for_each_whatprovides_id(other_dep_id, [&](auto) { called = true; });
                    REQUIRE_FALSE(called);
                }

                SECTION("Namespace names are in whatprovies")
                {
                    pool.add_dependency(pkg_name_id, REL_NAMESPACE, pkg_version_id);
                    pool.create_whatprovides();
                    bool called = false;
                    // Diff below in other_dep_id > pkg_name_id
                    pool.for_each_whatprovides_id(pkg_name_id, [&](auto) { called = true; });
                    REQUIRE(called);
                }
            }

            SECTION("Manually set whatprovides")
            {
                const auto dep_id = pool.add_string("mydep");

                SECTION("Without creating the whatprovides index is an error")
                {
                    REQUIRE_THROWS_AS(
                        pool.add_to_whatprovides(dep_id, pool.add_to_whatprovides_data({ id1 })),
                        std::runtime_error
                    );
                }

                SECTION("With creation of whatprovides index")
                {
                    pool.create_whatprovides();
                    pool.add_to_whatprovides(dep_id, pool.add_to_whatprovides_data({ id1 }));
                    auto whatprovides_ids = std::vector<SolvableId>();
                    pool.for_each_whatprovides_id(
                        dep_id,
                        [&](auto id) { whatprovides_ids.push_back(id); }
                    );
                    REQUIRE(whatprovides_ids == std::vector{ id1 });

                    SECTION("Gets cleared when calling create_whatprovides")
                    {
                        pool.create_whatprovides();
                        whatprovides_ids.clear();
                        pool.for_each_whatprovides_id(
                            dep_id,
                            [&](auto id) { whatprovides_ids.push_back(id); }
                        );
                        REQUIRE(whatprovides_ids.empty());
                    }
                }
            }
        }
    }

    SECTION("Add a debug callback")
    {
        std::string_view message = "";
        int type = 0;
        pool.set_debug_callback(
            [&](ObjPoolView /* pool */, auto t, auto msg) noexcept
            {
                message = msg;
                type = t;
            }
        );
        pool_debug(pool.raw(), SOLV_DEBUG_RESULT, "Ho no!");
        REQUIRE(message == "Ho no!");
        REQUIRE(type == SOLV_DEBUG_RESULT);
    }

    SECTION("Add a namespace callback")
    {
        pool.set_namespace_callback(
            [&](ObjPoolView /* pool */, StringId /* name */, StringId /* version */) noexcept -> OffsetId
            { return 0; }
        );
    }
}

TEST_CASE("Query Pool")
{
    auto pool = ObjPool();
    auto [repo_id, repo] = pool.add_repo("repo");

    auto [id1, s1] = repo.add_solvable();
    s1.set_name(pool.add_string("pkg"));
    s1.set_version(pool.add_string("2.0.0"));
    // Dependency foo>2.0
    s1.add_dependency(pool.add_dependency(pool.add_string("foo"), REL_GT, pool.add_string("2.0")));
    s1.add_self_provide();

    auto [id2, s2] = repo.add_solvable();
    s2.set_name(pool.add_string("pkg"));
    s2.set_version(pool.add_string("3.0.0"));
    // Dependency foo>3.0
    s2.add_dependency(pool.add_dependency(pool.add_string("foo"), REL_GT, pool.add_string("3.0")));
    s2.add_self_provide();

    repo.internalize();
    pool.create_whatprovides();  // Required otherwise segfault

    SECTION("Select Solvables")
    {
        SECTION("Resolving pkg>1.0.0")
        {
            const auto dep_id = pool.add_dependency(
                pool.add_string("pkg"),
                REL_GT,
                pool.add_string("1.0.0")
            );
            auto solvs = pool.select_solvables({ SOLVER_SOLVABLE_PROVIDES, dep_id });
            REQUIRE(solvs.size() == 2);
            REQUIRE(solvs.contains(id1));
            REQUIRE(solvs.contains(id2));
        }

        SECTION("Resolving pkg>2.1")
        {
            const auto dep_id = pool.add_dependency(
                pool.add_string("pkg"),
                REL_GT,
                pool.add_string("2.1")
            );
            auto solvs = pool.select_solvables({ SOLVER_SOLVABLE_PROVIDES, dep_id });
            REQUIRE(solvs.size() == 1);
            REQUIRE(solvs.contains(id2));
        }
    }

    SECTION("What matches dep")
    {
        SECTION("Who depends on a foo")
        {
            auto solvs = pool.what_matches_dep(SOLVABLE_REQUIRES, pool.add_string("foo"));
            REQUIRE(solvs.size() == 2);
            REQUIRE(solvs.contains(id1));
            REQUIRE(solvs.contains(id2));
        }

        SECTION("Who depends on a foo>4.0")
        {
            const auto dep_id = pool.add_dependency(
                pool.add_string("foo"),
                REL_GT,
                pool.add_string("4.0")
            );
            auto solvs = pool.what_matches_dep(SOLVABLE_REQUIRES, dep_id);
            REQUIRE(solvs.size() == 2);
            REQUIRE(solvs.contains(id1));
            REQUIRE(solvs.contains(id2));
        }

        SECTION("Who depends on foo<0.5")
        {
            const auto dep_id = pool.add_dependency(
                pool.add_string("foo"),
                REL_LT,
                pool.add_string("0.5")
            );
            auto solvs = pool.what_matches_dep(SOLVABLE_REQUIRES, dep_id);
            REQUIRE(solvs.empty());
        }
    }
}
