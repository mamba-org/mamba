// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <string>

#include <catch2/catch_all.hpp>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/repo.hpp"

using namespace solv;

/** Current timestamp in seconds. */
auto
timestamp() -> std::string
{
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );
    return std::to_string(seconds.count());
}

/** Unique  temporary directory deleted after tests. */
struct TmpDir
{
    std::filesystem::path path;

    TmpDir(std::filesystem::path path_)
        : path(std::move(path_))
    {
        std::filesystem::create_directories(path);
    }

    TmpDir()
        : TmpDir(std::filesystem::temp_directory_path() / "solv-cpp/tests" / timestamp())
    {
    }

    ~TmpDir()
    {
        std::filesystem::remove_all(path);
    }
};

namespace
{
    TEST_CASE("Construct a repo", "[solv::ObjRepo]")
    {
        auto pool = ObjPool();
        auto [repo_id, repo] = pool.add_repo("test-forge");
        REQUIRE(repo.id() == repo_id);
        REQUIRE(repo.name() == "test-forge");

        SECTION("Fetch the repo")
        {
            REQUIRE(pool.has_repo(repo_id));
            auto repo_alt = pool.get_repo(repo_id).value();
            REQUIRE(repo_alt.name() == repo.name());
            REQUIRE(repo_alt.id() == repo.id());
        }

        SECTION("Set attributes")
        {
            repo.set_url("https://repo.mamba.pm/conda-forge");
            repo.set_etag(R"(etag)W/"8eea3023872b68ef71fd930472a15599"(etag)");
            repo.set_mod("Tue, 25 Apr 2023 11:48:37 GMT");
            repo.set_channel("conda-forge");
            repo.set_subdir("noarch");
            repo.set_pip_added(true);
            repo.set_tool_version("1.2.3.4");

            SECTION("Empty without internalize")
            {
                REQUIRE(repo.url() == "");
                REQUIRE(repo.etag() == "");
                REQUIRE(repo.mod() == "");
                REQUIRE(repo.channel() == "");
                REQUIRE(repo.subdir() == "");
                REQUIRE(repo.pip_added() == false);
                REQUIRE(repo.tool_version() == "");
            }

            SECTION("Internalize and get attributes")
            {
                repo.internalize();

                REQUIRE(repo.url() == "https://repo.mamba.pm/conda-forge");
                REQUIRE(repo.channel() == "conda-forge");
                REQUIRE(repo.subdir() == "noarch");
                REQUIRE(repo.etag() == R"(etag)W/"8eea3023872b68ef71fd930472a15599"(etag)");
                REQUIRE(repo.mod() == "Tue, 25 Apr 2023 11:48:37 GMT");
                REQUIRE(repo.pip_added() == true);
                REQUIRE(repo.tool_version() == "1.2.3.4");

                SECTION("Override attribute")
                {
                    repo.set_subdir("linux-64");
                    REQUIRE(repo.subdir() == "noarch");
                    repo.internalize();
                    REQUIRE(repo.subdir() == "linux-64");
                }
            }
        }

        SECTION("Add solvables")
        {
            REQUIRE(repo.solvable_count() == 0);
            const auto [id1, s1] = repo.add_solvable();
            REQUIRE(repo.get_solvable(id1).has_value());
            REQUIRE(repo.get_solvable(id1)->raw() == s1.raw());
            REQUIRE(repo.solvable_count() == 1);
            REQUIRE(repo.has_solvable(id1));
            const auto [id2, s2] = repo.add_solvable();
            REQUIRE(repo.solvable_count() == 2);
            REQUIRE(repo.has_solvable(id2));

            SECTION("Retrieve repo from solvable")
            {
                REQUIRE(ObjRepoViewConst::of_solvable(s1).raw() == repo.raw());
            }

            SECTION("Iterate over solvables")
            {
                SECTION("Over all solvables")
                {
                    const auto ids = std::array{ id1, id2 };
                    std::size_t n_solvables = 0;
                    repo.for_each_solvable_id(
                        [&](SolvableId id)
                        {
                            REQUIRE(std::find(ids.cbegin(), ids.cend(), id) != ids.cend());
                            n_solvables++;
                        }
                    );
                    REQUIRE(n_solvables == repo.solvable_count());
                }

                SECTION("Over one solvable then break")
                {
                    std::size_t n_solvables = 0;
                    repo.for_each_solvable(
                        [&](ObjSolvableView)
                        {
                            n_solvables++;
                            return LoopControl::Break;
                        }
                    );
                    REQUIRE(n_solvables == 1);
                }
            }

            SECTION("Get inexisting solvable")
            {
                REQUIRE_FALSE(repo.has_solvable(1234));
                REQUIRE_FALSE(repo.get_solvable(1234).has_value());
            }

            SECTION("Remove solvable")
            {
                REQUIRE(repo.remove_solvable(id2, true));
                REQUIRE_FALSE(repo.has_solvable(id2));
                REQUIRE(repo.has_solvable(id1));
                REQUIRE(repo.solvable_count() == 1);
            }

            SECTION("Confuse ids from another repo")
            {
                auto [other_repo_id, other_repo] = pool.add_repo("other-repo");
                auto [other_id, other_s] = other_repo.add_solvable();

                REQUIRE_FALSE(repo.has_solvable(other_id));
                REQUIRE_FALSE(repo.get_solvable(other_id).has_value());
                REQUIRE_FALSE(repo.remove_solvable(other_id, true));
            }

            SECTION("Clear solvables")
            {
                repo.clear(true);
                REQUIRE(repo.solvable_count() == 0);
                REQUIRE_FALSE(repo.has_solvable(id1));
                REQUIRE_FALSE(repo.get_solvable(id1).has_value());
            }

            SECTION("Write repo to file")
            {
                // Using only C OS encoding API for test.
                auto dir = TmpDir();
                const auto solv_file = (dir.path / "test-forge.solv").string();
                {
                    std::FILE* fptr = std::fopen(solv_file.c_str(), "wb");
                    REQUIRE(fptr != nullptr);
                    const auto written = repo.write(fptr);
                    REQUIRE(written);
                    REQUIRE(std::fclose(fptr) == 0);
                }

                SECTION("Read repo from file")
                {
                    // Delete repo
                    const auto n_solvables = repo.solvable_count();
                    pool.remove_repo(repo_id, true);

                    // Create new repo from file
                    auto [repo_id2, repo2] = pool.add_repo("test-forge");
                    std::FILE* fptr = std::fopen(solv_file.c_str(), "rb");
                    REQUIRE(fptr != nullptr);
                    const auto read = repo2.read(fptr);
                    REQUIRE(read);
                    REQUIRE(std::fclose(fptr) == 0);

                    REQUIRE(repo2.solvable_count() == n_solvables);
                    // True because we reused ids
                    REQUIRE(repo2.has_solvable(id1));
                    REQUIRE(repo2.has_solvable(id2));
                }
            }
        }
    }
}
