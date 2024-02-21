// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <type_traits>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/specs/channel.hpp"

#include "mambatests.hpp"

using namespace mamba;
using namespace mamba::solver;

auto
find_actions_with_name(const Solution& solution, std::string_view name)
    -> std::vector<Solution::Action>
{
    auto out = std::vector<Solution::Action>();
    for (const auto& action : solution.actions)
    {
        std::visit(
            [&](const auto& act)
            {
                using Act = std::decay_t<decltype(act)>;
                if constexpr (Solution::has_remove_v<Act>)
                {
                    if (act.remove.name == name)
                    {
                        out.push_back(act);
                    }
                }
                else if constexpr (Solution::has_install_v<Act>)
                {
                    if (act.install.name == name)
                    {
                        out.push_back(act);
                    }
                }
                else
                {
                    if (act.what.name == name)
                    {
                        out.push_back(act);
                    }
                }
            },
            action
        );
    }
    return out;
}

TEST_SUITE("solver::libsolv::solver")
{
    using namespace specs::match_spec_literals;

    TEST_CASE("Solve a fresh environment with one repository")
    {
        auto db = libsolv::Database({});

        // A conda-forge/linux-64 subsample with one version of numpy and pip and their dependencies
        auto repo = db.add_repo_from_repodata_json(
            mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
            "https://conda.anaconda.org/conda-forge/linux-64",
            libsolv::PipAsPythonDependency::No
        );
        REQUIRE(repo.has_value());

        SUBCASE("Install numpy")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "numpy"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE_FALSE(solution.actions.empty());
            // Numpy is last because of topological sort
            CHECK(std::holds_alternative<Solution::Install>(solution.actions.back()));
            CHECK_EQ(std::get<Solution::Install>(solution.actions.back()).install.name, "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE_EQ(python_actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Install>(python_actions.front()));
        }

        SUBCASE("Install numpy without dependencies")
        {
            const auto request = Request{
                /* .flags= */ {
                    /* .keep_dependencies */ false,
                    /* .keep_user_specs */ true,
                },
                /* .jobs= */ { Request::Install{ "numpy"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE_FALSE(solution.actions.empty());
            // Numpy is last because of topological sort
            CHECK(std::holds_alternative<Solution::Install>(solution.actions.back()));
            CHECK_EQ(std::get<Solution::Install>(solution.actions.back()).install.name, "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE_EQ(python_actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Omit>(python_actions.front()));
        }

        SUBCASE("Install numpy dependencies only")
        {
            const auto request = Request{
                /* .flags= */ {
                    /* .keep_dependencies */ true,
                    /* .keep_user_specs */ false,
                },
                /* .jobs= */ { Request::Install{ "numpy"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE_FALSE(solution.actions.empty());
            // Numpy is last because of topological sort
            CHECK(std::holds_alternative<Solution::Omit>(solution.actions.back()));
            CHECK_EQ(std::get<Solution::Omit>(solution.actions.back()).what.name, "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE_EQ(python_actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Install>(python_actions.front()));

            // Pip is not a dependency of numpy (or python here)
            CHECK(find_actions_with_name(solution, "pip").empty());
        }

        SUBCASE("Fail to install missing package")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "does-not-exist"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<libsolv::UnSolvable>(outcome.value()));
        }
    }

    TEST_CASE("Remove packages")
    {
        auto db = libsolv::Database({});

        // A conda-forge/linux-64 subsample with one version of numpy and pip and their dependencies
        const auto repo = db.add_repo_from_repodata_json(
            mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
            "https://conda.anaconda.org/conda-forge/linux-64"
        );
        REQUIRE(repo.has_value());
        db.set_installed_repo(repo.value());

        SUBCASE("Remove numpy and dependencies")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Remove{ "numpy"_ms, true } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE_FALSE(solution.actions.empty());
            // Numpy is first because of topological sort
            CHECK(std::holds_alternative<Solution::Remove>(solution.actions.front()));
            CHECK_EQ(std::get<Solution::Remove>(solution.actions.front()).remove.name, "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            // Python is not removed because it is needed by pip which is installed
            CHECK(find_actions_with_name(solution, "pip").empty());
        }

        SUBCASE("Remove numpy and pip and dependencies")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Remove{ "numpy"_ms, true }, Request::Remove{ "pip"_ms, true } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto numpy_actions = find_actions_with_name(solution, "numpy");
            REQUIRE_EQ(numpy_actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Remove>(numpy_actions.front()));

            const auto pip_actions = find_actions_with_name(solution, "pip");
            REQUIRE_EQ(pip_actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Remove>(pip_actions.front()));

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE_EQ(python_actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Remove>(python_actions.front()));
        }

        SUBCASE("Remove numpy without dependencies")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Remove{ "numpy"_ms, false } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE_EQ(solution.actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Remove>(solution.actions.front()));
            CHECK_EQ(std::get<Solution::Remove>(solution.actions.front()).remove.name, "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);
        }

        SUBCASE("Removing non-existing package is a no-op")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Remove{ "does-not-exist"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            CHECK(solution.actions.empty());
        }
    }
}
