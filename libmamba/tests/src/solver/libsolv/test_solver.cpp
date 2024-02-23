// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <type_traits>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/fs/filesystem.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/package_info.hpp"
#include "mamba/util/string.hpp"

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
        const auto repo = db.add_repo_from_repodata_json(
            mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
            "https://conda.anaconda.org/conda-forge/linux-64",
            "conda-forge",
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


        SUBCASE("Fail to install conflicting dependencies")
        {
            const auto request = Request{
                /* .flags= */ {
                    /* .keep_dependencies */ true,
                    /* .keep_user_specs */ false,
                },
                /* .jobs= */ { Request::Install{ "numpy"_ms }, Request::Install{ "python=2.7"_ms } },
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
            "https://conda.anaconda.org/conda-forge/linux-64",
            "conda-forge"
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

    TEST_CASE("Solve a existing environment with one repository")
    {
        auto db = libsolv::Database({});

        // A conda-forge/linux-64 subsample with one version of numpy and pip and their dependencies
        const auto repo = db.add_repo_from_repodata_json(
            mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
            "https://conda.anaconda.org/conda-forge/linux-64",
            "conda-forge",
            libsolv::PipAsPythonDependency::No
        );
        REQUIRE(repo.has_value());

        SUBCASE("numpy 1.0 is installed")
        {
            const auto installed = db.add_repo_from_packages(std::array{
                specs::PackageInfo("numpy", "1.0.0", "phony", 0),
            });
            db.set_installed_repo(installed);

            SUBCASE("Installing numpy does not upgrade")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "numpy"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());
                CHECK(solution.actions.empty());
            }

            SUBCASE("Upgrade numpy")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Update{ "numpy"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                REQUIRE_FALSE(solution.actions.empty());
                // Numpy is last because of topological sort
                CHECK(std::holds_alternative<Solution::Upgrade>(solution.actions.back()));
                CHECK_EQ(std::get<Solution::Upgrade>(solution.actions.back()).install.name, "numpy");
                CHECK_EQ(std::get<Solution::Upgrade>(solution.actions.back()).install.version, "1.26.4");
                CHECK_EQ(std::get<Solution::Upgrade>(solution.actions.back()).remove.version, "1.0.0");
                REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

                // Python needs to be installed
                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE_EQ(python_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Install>(python_actions.front()));
            }

            SUBCASE("Update numpy to no better solution is a no-op")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Update{ "numpy<=1.1"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());
                CHECK(solution.actions.empty());
            }
        }

        SUBCASE("numpy 1.0 is installed with python 2.0 and foo")
        {
            auto pkg_numpy = specs::PackageInfo("numpy", "1.0.0", "phony", 0);
            pkg_numpy.depends = { "python=2.0", "foo" };
            const auto installed = db.add_repo_from_packages(std::array{
                pkg_numpy,
                specs::PackageInfo("python", "2.0.0", "phony", 0),
                specs::PackageInfo("foo"),
            });
            db.set_installed_repo(installed);

            SUBCASE("numpy is upgraded with cleaning dependencies")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Update{ "numpy"_ms, true } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto numpy_actions = find_actions_with_name(solution, "numpy");
                REQUIRE_EQ(numpy_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).install.version, "1.26.4");
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version, "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE_EQ(python_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(python_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(python_actions.front()).install.version, "3.12.1");
                CHECK_EQ(std::get<Solution::Upgrade>(python_actions.front()).remove.version, "2.0.0");

                const auto foo_actions = find_actions_with_name(solution, "foo");
                REQUIRE_EQ(foo_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Remove>(foo_actions.front()));
            }

            SUBCASE("numpy is upgraded with cleaning dependencies and a user keep")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Update{ "numpy"_ms, true }, Request::Keep{ "foo"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto numpy_actions = find_actions_with_name(solution, "numpy");
                REQUIRE_EQ(numpy_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).install.version, "1.26.4");
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version, "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE_EQ(python_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(python_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(python_actions.front()).install.version, "3.12.1");
                CHECK_EQ(std::get<Solution::Upgrade>(python_actions.front()).remove.version, "2.0.0");

                // foo is left unchanged in the installed repository because of Keep job
                CHECK(find_actions_with_name(solution, "foo").empty());
            }

            SUBCASE("numpy is upgraded without cleaning dependencies")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Update{ "numpy"_ms, false } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto numpy_actions = find_actions_with_name(solution, "numpy");
                REQUIRE_EQ(numpy_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).install.version, "1.26.4");
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version, "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE_EQ(python_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(python_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(python_actions.front()).install.version, "3.12.1");
                CHECK_EQ(std::get<Solution::Upgrade>(python_actions.front()).remove.version, "2.0.0");

                CHECK(find_actions_with_name(solution, "foo").empty());
            }

            SUBCASE("python upgrade leads to numpy upgrade")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Update{ "python"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto numpy_actions = find_actions_with_name(solution, "numpy");
                REQUIRE_EQ(numpy_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).install.version, "1.26.4");
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version, "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE_EQ(python_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(python_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(python_actions.front()).install.version, "3.12.1");
                CHECK_EQ(std::get<Solution::Upgrade>(python_actions.front()).remove.version, "2.0.0");

                CHECK(find_actions_with_name(solution, "foo").empty());
            }
        }

        SUBCASE("numpy 1.0 is installed with python 4.0 and constrained foo")
        {
            auto pkg_numpy = specs::PackageInfo("numpy", "1.0.0", "phony", 0);
            pkg_numpy.depends = { "python=4.0", "foo" };
            auto pkg_foo = specs::PackageInfo("foo", "1.0.0", "phony", 0);
            pkg_foo.constrains = { "numpy=1.0.0", "foo" };
            const auto installed = db.add_repo_from_packages(std::array{
                pkg_numpy,
                pkg_foo,
                specs::PackageInfo("python", "4.0.0", "phony", 0),
            });
            db.set_installed_repo(installed);

            SUBCASE("numpy upgrade lead to allowed python downgrade")
            {
                const auto request = Request{
                    /* .flags= */ {
                        /* .keep_dependencies= */ true,
                        /* .keep_user_specs= */ true,
                        /* .force_reinstall= */ false,
                        /* .allow_downgrade= */ true,
                        /* .allow_uninstall= */ true,
                    },
                    /* .jobs= */ { Request::Update{ "numpy"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto numpy_actions = find_actions_with_name(solution, "numpy");
                REQUIRE_EQ(numpy_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).install.version, "1.26.4");
                CHECK_EQ(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version, "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE_EQ(python_actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Downgrade>(python_actions.front()));
                CHECK_EQ(std::get<Solution::Downgrade>(python_actions.front()).install.version, "3.12.1");
                CHECK_EQ(std::get<Solution::Downgrade>(python_actions.front()).remove.version, "4.0.0");
            }

            SUBCASE("no numpy upgrade without allowing downgrading other packages")
            {
                const auto request = Request{
                    /* .flags= */ {
                        /* .keep_dependencies= */ true,
                        /* .keep_user_specs= */ true,
                        /* .force_reinstall= */ false,
                        /* .allow_downgrade= */ false,
                        /* .allow_uninstall= */ true,
                    },
                    /* .jobs= */ { Request::Update{ "numpy"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                // No possible changes
                CHECK(solution.actions.empty());
            }
        }
    }

    TEST_CASE("Solve a fresh environment with multiple repositories")
    {
        auto db = libsolv::Database({});

        const auto repo1 = db.add_repo_from_packages(std::array{
            specs::PackageInfo("numpy", "1.0.0", "repo1", 0),
        });
        const auto repo2 = db.add_repo_from_packages(std::array{
            specs::PackageInfo("numpy", "2.0.0", "repo2", 0),
        });
        db.set_repo_priority(repo1, { 2, 0 });
        db.set_repo_priority(repo2, { 1, 0 });

        SUBCASE("All repos considered without strict repo priority")
        {
            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "numpy>=2.0"_ms } },
            };
            request.flags.strict_repo_priority = false;
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto numpy_actions = find_actions_with_name(solution, "numpy");
            REQUIRE_EQ(numpy_actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Install>(numpy_actions.front()));
            CHECK_EQ(std::get<Solution::Install>(numpy_actions.front()).install.version, "2.0.0");
        }

        SUBCASE("Fail to get package from non priority repo with strict repo priority")
        {
            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "numpy>=2.0"_ms } },
            };
            request.flags.strict_repo_priority = true;
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            CHECK(std::holds_alternative<libsolv::UnSolvable>(outcome.value()));
        }
    }

    TEST_CASE("Install highest priority package")
    {
        auto db = libsolv::Database({});

        auto mkfoo = [](std::string version,
                        std::size_t build_number = 0,
                        std::vector<std::string> track_features = {},
                        std::size_t timestamp = 0) -> specs::PackageInfo
        {
            auto out = specs::PackageInfo("foo");
            out.version = std::move(version);
            out.build_number = build_number;
            out.track_features = std::move(track_features);
            out.timestamp = timestamp;
            return out;
        };

        SUBCASE("Pins are respected")
        {
            db.add_repo_from_packages(std::array{
                mkfoo("1.0.0", 0, { "feat" }, 0),
                mkfoo("2.0.0", 1, {}, 1),
            });

            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo"_ms }, Request::Pin{ "foo==1.0"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto actions = find_actions_with_name(solution, "foo");
            REQUIRE_EQ(actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Install>(actions.front()));
            CHECK_EQ(std::get<Solution::Install>(actions.front()).install.version, "1.0.0");
        }

        SUBCASE("Track features has highest priority")
        {
            db.add_repo_from_packages(std::array{
                mkfoo("1.0.0", 0, {}, 0),
                mkfoo("2.0.0", 1, { "feat" }, 1),
            });
            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto actions = find_actions_with_name(solution, "foo");
            REQUIRE_EQ(actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Install>(actions.front()));
            CHECK_EQ(std::get<Solution::Install>(actions.front()).install.version, "1.0.0");
        }

        SUBCASE("Version has second highest priority")
        {
            db.add_repo_from_packages(std::array{
                mkfoo("2.0.0", 0, {}, 0),
                mkfoo("1.0.0", 1, {}, 1),
            });
            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto actions = find_actions_with_name(solution, "foo");
            REQUIRE_EQ(actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Install>(actions.front()));
            CHECK_EQ(std::get<Solution::Install>(actions.front()).install.version, "2.0.0");
        }

        SUBCASE("Build number has third highest priority")
        {
            db.add_repo_from_packages(std::array{
                mkfoo("2.0.0", 1, {}, 0),
                mkfoo("2.0.0", 0, {}, 1),
            });
            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto actions = find_actions_with_name(solution, "foo");
            REQUIRE_EQ(actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Install>(actions.front()));
            CHECK_EQ(std::get<Solution::Install>(actions.front()).install.build_number, 1);
        }

        SUBCASE("Timestamp has lowest priority")
        {
            db.add_repo_from_packages(std::array{
                mkfoo("2.0.0", 0, {}, 0),
                mkfoo("2.0.0", 0, {}, 1),
            });
            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto actions = find_actions_with_name(solution, "foo");
            REQUIRE_EQ(actions.size(), 1);
            CHECK(std::holds_alternative<Solution::Install>(actions.front()));
            CHECK_EQ(std::get<Solution::Install>(actions.front()).install.timestamp, 1);
        }
    }

    TEST_CASE("Respect channel-specific MatchSpec")
    {
        auto db = libsolv::Database({
            /* .platforms= */ { "linux-64", "noarch" },
            /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/"),
        });

        SUBCASE("Different channels")
        {
            auto pkg1 = specs::PackageInfo("foo", "1.0.0", "conda", 0);
            pkg1.package_url = "https://conda.anaconda.org/conda-forge/linux-64/foo-1.0.0-phony.conda";
            db.add_repo_from_packages(std::array{ pkg1 });
            auto pkg2 = specs::PackageInfo("foo", "1.0.0", "mamba", 0);
            pkg2.package_url = "https://conda.anaconda.org/mamba-forge/linux-64/foo-1.0.0-phony.conda";
            db.add_repo_from_packages(std::array{ pkg2 });

            SUBCASE("conda-forge")
            {
                auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "conda-forge::foo"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto actions = find_actions_with_name(solution, "foo");
                REQUIRE_EQ(actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Install>(actions.front()));
                CHECK_EQ(std::get<Solution::Install>(actions.front()).install.build_string, "conda");
            }

            SUBCASE("mamba-forge")
            {
                auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "mamba-forge::foo"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto actions = find_actions_with_name(solution, "foo");
                REQUIRE_EQ(actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Install>(actions.front()));
                CHECK_EQ(std::get<Solution::Install>(actions.front()).install.build_string, "mamba");
            }

            SUBCASE("pixi-forge")
            {
                auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "pixi-forge::foo"_ms } },
                };

                // TODO should really be an unsolvable state
                CHECK_THROWS(libsolv::Solver().solve(db, request));
            }

            SUBCASE("https://conda.anaconda.org/mamba-forge/")
            {
                auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "https://conda.anaconda.org/mamba-forge::foo"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto actions = find_actions_with_name(solution, "foo");
                REQUIRE_EQ(actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Install>(actions.front()));
                CHECK_EQ(std::get<Solution::Install>(actions.front()).install.build_string, "mamba");
            }
        }

        SUBCASE("Different subdirs")
        {
            const auto repo_linux = db.add_repo_from_repodata_json(
                mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No
            );
            REQUIRE(repo_linux.has_value());

            const auto repo_win = db.add_repo_from_repodata_json(
                mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
                "https://conda.anaconda.org/conda-forge/noarch",
                "conda-forge",
                libsolv::PipAsPythonDependency::No
            );
            REQUIRE(repo_win.has_value());

            SUBCASE("conda-forge/noarch")
            {
                auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "conda-forge/noarch::numpy"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto actions = find_actions_with_name(solution, "numpy");
                REQUIRE_EQ(actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Install>(actions.front()));
                CHECK(util::contains(
                    std::get<Solution::Install>(actions.front()).install.package_url,
                    "noarch"
                ));
            }

            SUBCASE("conda-forge[subdir=linux-64]")
            {
                auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "conda-forge::numpy[subdir=linux-64]"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());

                const auto actions = find_actions_with_name(solution, "numpy");
                REQUIRE_EQ(actions.size(), 1);
                CHECK(std::holds_alternative<Solution::Install>(actions.front()));
                CHECK(util::contains(
                    std::get<Solution::Install>(actions.front()).install.package_url,
                    "linux-64"
                ));
            }
        }
    }
}
