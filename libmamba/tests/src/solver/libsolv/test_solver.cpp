// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <type_traits>
#include <variant>
#include <vector>

#include <catch2/catch_all.hpp>

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

namespace
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

        SECTION("Install numpy")
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
            REQUIRE(std::holds_alternative<Solution::Install>(solution.actions.back()));
            REQUIRE(std::get<Solution::Install>(solution.actions.back()).install.name == "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE(python_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(python_actions.front()));
        }

        SECTION("Force reinstall not installed numpy")
        {
            auto flags = Request::Flags();
            flags.force_reinstall = true;
            const auto request = Request{
                /* .flags= */ std::move(flags),
                /* .jobs= */ { Request::Install{ "numpy"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE_FALSE(solution.actions.empty());
            // Numpy is last because of topological sort
            REQUIRE(std::holds_alternative<Solution::Install>(solution.actions.back()));
            REQUIRE(std::get<Solution::Install>(solution.actions.back()).install.name == "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE(python_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(python_actions.front()));
        }

        SECTION("Install numpy without dependencies")
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
            REQUIRE(std::holds_alternative<Solution::Install>(solution.actions.back()));
            REQUIRE(std::get<Solution::Install>(solution.actions.back()).install.name == "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE(python_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Omit>(python_actions.front()));
        }

        SECTION("Install numpy dependencies only")
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
            REQUIRE(std::holds_alternative<Solution::Omit>(solution.actions.back()));
            REQUIRE(std::get<Solution::Omit>(solution.actions.back()).what.name == "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE(python_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(python_actions.front()));

            // Pip is not a dependency of numpy (or python here)
            REQUIRE(find_actions_with_name(solution, "pip").empty());
        }

        SECTION("Fail to install missing package")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "does-not-exist"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<libsolv::UnSolvable>(outcome.value()));
        }


        SECTION("Fail to install conflicting dependencies")
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

        SECTION("Remove numpy and dependencies")
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
            REQUIRE(std::holds_alternative<Solution::Remove>(solution.actions.front()));
            REQUIRE(std::get<Solution::Remove>(solution.actions.front()).remove.name == "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

            // Python is not removed because it is needed by pip which is installed
            REQUIRE(find_actions_with_name(solution, "pip").empty());
        }

        SECTION("Remove numpy and pip and dependencies")
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
            REQUIRE(numpy_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Remove>(numpy_actions.front()));

            const auto pip_actions = find_actions_with_name(solution, "pip");
            REQUIRE(pip_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Remove>(pip_actions.front()));

            const auto python_actions = find_actions_with_name(solution, "python");
            REQUIRE(python_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Remove>(python_actions.front()));
        }

        SECTION("Remove numpy without dependencies")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Remove{ "numpy"_ms, false } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE(solution.actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Remove>(solution.actions.front()));
            REQUIRE(std::get<Solution::Remove>(solution.actions.front()).remove.name == "numpy");
            REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);
        }

        SECTION("Removing non-existing package is a no-op")
        {
            const auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Remove{ "does-not-exist"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE(solution.actions.empty());
        }
    }

    TEST_CASE("Reinstall packages")
    {
        auto db = libsolv::Database({});

        // A conda-forge/linux-64 subsample with one version of numpy and pip and their dependencies
        const auto repo_installed = db.add_repo_from_repodata_json(
            mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
            "installed",
            "installed"
        );
        REQUIRE(repo_installed.has_value());
        db.set_installed_repo(repo_installed.value());
        const auto repo = db.add_repo_from_repodata_json(
            mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
            "https://conda.anaconda.org/conda-forge/linux-64",
            "conda-forge"
        );
        REQUIRE(repo.has_value());

        SECTION("Force reinstall numpy resinstalls it")
        {
            auto flags = Request::Flags();
            flags.force_reinstall = true;
            const auto request = Request{
                /* .flags= */ std::move(flags),
                /* .jobs= */ { Request::Install{ "numpy"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE(solution.actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Reinstall>(solution.actions.front()));
            REQUIRE(std::get<Solution::Reinstall>(solution.actions.front()).what.name == "numpy");
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

        SECTION("numpy 1.0 is installed")
        {
            const auto installed = db.add_repo_from_packages(std::array{
                specs::PackageInfo("numpy", "1.0.0", "phony", 0),
            });
            db.set_installed_repo(installed);

            SECTION("Installing numpy does not upgrade")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "numpy"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());
                REQUIRE(solution.actions.empty());
            }

            SECTION("Upgrade numpy")
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
                REQUIRE(std::holds_alternative<Solution::Upgrade>(solution.actions.back()));
                REQUIRE(std::get<Solution::Upgrade>(solution.actions.back()).install.name == "numpy");
                REQUIRE(
                    std::get<Solution::Upgrade>(solution.actions.back()).install.version == "1.26.4"
                );
                REQUIRE(std::get<Solution::Upgrade>(solution.actions.back()).remove.version == "1.0.0");
                REQUIRE_EQ(find_actions_with_name(solution, "numpy").size(), 1);

                // Python needs to be installed
                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE(python_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Install>(python_actions.front()));
            }

            SECTION("Update numpy to no better solution is a no-op")
            {
                const auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Update{ "numpy<=1.1"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<Solution>(outcome.value()));
                const auto& solution = std::get<Solution>(outcome.value());
                REQUIRE(solution.actions.empty());
            }
        }

        SECTION("numpy 1.0 is installed with python 2.0 and foo")
        {
            auto pkg_numpy = specs::PackageInfo("numpy", "1.0.0", "phony", 0);
            pkg_numpy.dependencies = { "python=2.0", "foo" };
            const auto installed = db.add_repo_from_packages(std::array{
                pkg_numpy,
                specs::PackageInfo("python", "2.0.0", "phony", 0),
                specs::PackageInfo("foo"),
            });
            db.set_installed_repo(installed);

            SECTION("numpy is upgraded with cleaning dependencies")
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
                REQUIRE(numpy_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).install.version == "1.26.4");
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version == "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE(python_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(python_actions.front()));
                REQUIRE(
                    std::get<Solution::Upgrade>(python_actions.front()).install.version == "3.12.1"
                );
                REQUIRE(std::get<Solution::Upgrade>(python_actions.front()).remove.version == "2.0.0");

                const auto foo_actions = find_actions_with_name(solution, "foo");
                REQUIRE(foo_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Remove>(foo_actions.front()));
            }

            SECTION("numpy is upgraded with cleaning dependencies and a user keep")
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
                REQUIRE(numpy_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).install.version == "1.26.4");
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version == "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE(python_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(python_actions.front()));
                REQUIRE(
                    std::get<Solution::Upgrade>(python_actions.front()).install.version == "3.12.1"
                );
                REQUIRE(std::get<Solution::Upgrade>(python_actions.front()).remove.version == "2.0.0");

                // foo is left unchanged in the installed repository because of Keep job
                REQUIRE(find_actions_with_name(solution, "foo").empty());
            }

            SECTION("numpy is upgraded without cleaning dependencies")
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
                REQUIRE(numpy_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).install.version == "1.26.4");
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version == "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE(python_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(python_actions.front()));
                REQUIRE(
                    std::get<Solution::Upgrade>(python_actions.front()).install.version == "3.12.1"
                );
                REQUIRE(std::get<Solution::Upgrade>(python_actions.front()).remove.version == "2.0.0");

                REQUIRE(find_actions_with_name(solution, "foo").empty());
            }

            SECTION("python upgrade leads to numpy upgrade")
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
                REQUIRE(numpy_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).install.version == "1.26.4");
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version == "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE(python_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(python_actions.front()));
                REQUIRE(
                    std::get<Solution::Upgrade>(python_actions.front()).install.version == "3.12.1"
                );
                REQUIRE(std::get<Solution::Upgrade>(python_actions.front()).remove.version == "2.0.0");

                REQUIRE(find_actions_with_name(solution, "foo").empty());
            }
        }

        SECTION("numpy 1.0 is installed with python 4.0 and constrained foo")
        {
            auto pkg_numpy = specs::PackageInfo("numpy", "1.0.0", "phony", 0);
            pkg_numpy.dependencies = { "python=4.0", "foo" };
            auto pkg_foo = specs::PackageInfo("foo", "1.0.0", "phony", 0);
            pkg_foo.constrains = { "numpy=1.0.0", "foo" };
            const auto installed = db.add_repo_from_packages(std::array{
                pkg_numpy,
                pkg_foo,
                specs::PackageInfo("python", "4.0.0", "phony", 0),
            });
            db.set_installed_repo(installed);

            SECTION("numpy upgrade lead to allowed python downgrade")
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
                REQUIRE(numpy_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Upgrade>(numpy_actions.front()));
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).install.version == "1.26.4");
                REQUIRE(std::get<Solution::Upgrade>(numpy_actions.front()).remove.version == "1.0.0");

                const auto python_actions = find_actions_with_name(solution, "python");
                REQUIRE(python_actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Downgrade>(python_actions.front()));
                REQUIRE(
                    std::get<Solution::Downgrade>(python_actions.front()).install.version == "3.12.1"
                );
                REQUIRE(
                    std::get<Solution::Downgrade>(python_actions.front()).remove.version == "4.0.0"
                );
            }

            SECTION("no numpy upgrade without allowing downgrading other packages")
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
                REQUIRE(solution.actions.empty());
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

        SECTION("All repos considered without strict repo priority")
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
            REQUIRE(numpy_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(numpy_actions.front()));
            REQUIRE(std::get<Solution::Install>(numpy_actions.front()).install.version == "2.0.0");
        }

        SECTION("Fail to get package from non priority repo with strict repo priority")
        {
            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "numpy>=2.0"_ms } },
            };
            request.flags.strict_repo_priority = true;
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<libsolv::UnSolvable>(outcome.value()));
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

        SECTION("Pins are respected")
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
            REQUIRE(actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
            REQUIRE(std::get<Solution::Install>(actions.front()).install.version == "1.0.0");
        }

        SECTION("Track features has highest priority")
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
            REQUIRE(actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
            REQUIRE(std::get<Solution::Install>(actions.front()).install.version == "1.0.0");
        }

        SECTION("Version has second highest priority")
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
            REQUIRE(actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
            REQUIRE(std::get<Solution::Install>(actions.front()).install.version == "2.0.0");
        }

        SECTION("Build number has third highest priority")
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
            REQUIRE(actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
            REQUIRE(std::get<Solution::Install>(actions.front()).install.build_number == 1);
        }

        SECTION("Timestamp has lowest priority")
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
            REQUIRE(actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
            REQUIRE(std::get<Solution::Install>(actions.front()).install.timestamp == 1);
        }
    }

    TEST_CASE("Respect channel-specific MatchSpec")
    {
        auto db = libsolv::Database({
            /* .platforms= */ { "linux-64", "noarch" },
            /* .channel_alias= */ specs::CondaURL::parse("https://conda.anaconda.org/").value(),
        });

        SECTION("Different channels")
        {
            auto pkg1 = specs::PackageInfo("foo", "1.0.0", "conda", 0);
            pkg1.package_url = "https://conda.anaconda.org/conda-forge/linux-64/foo-1.0.0-phony.conda";
            db.add_repo_from_packages(std::array{ pkg1 });
            auto pkg2 = specs::PackageInfo("foo", "1.0.0", "mamba", 0);
            pkg2.package_url = "https://conda.anaconda.org/mamba-forge/linux-64/foo-1.0.0-phony.conda";
            db.add_repo_from_packages(std::array{ pkg2 });

            SECTION("conda-forge::foo")
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
                REQUIRE(actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
                REQUIRE(std::get<Solution::Install>(actions.front()).install.build_string == "conda");
            }

            SECTION("mamba-forge::foo")
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
                REQUIRE(actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
                REQUIRE(std::get<Solution::Install>(actions.front()).install.build_string == "mamba");
            }

            SECTION("pixi-forge::foo")
            {
                auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "pixi-forge::foo"_ms } },
                };

                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<libsolv::UnSolvable>(outcome.value()));
            }

            SECTION("https://conda.anaconda.org/mamba-forge::foo")
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
                REQUIRE(actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
                REQUIRE(std::get<Solution::Install>(actions.front()).install.build_string == "mamba");
            }
        }

        SECTION("Different subdirs")
        {
            const auto repo_linux = db.add_repo_from_repodata_json(
                mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
                "https://conda.anaconda.org/conda-forge/linux-64",
                "conda-forge",
                libsolv::PipAsPythonDependency::No
            );
            REQUIRE(repo_linux.has_value());

            // FIXME the subdir is not overridden here so it is still linux-64 because that's what
            // is in the json file.
            // We'de want to pass option to the database to override channel and subsir.
            const auto repo_noarch = db.add_repo_from_repodata_json(
                mambatests::test_data_dir / "repodata/conda-forge-numpy-linux-64.json",
                "https://conda.anaconda.org/conda-forge/noarch",
                "conda-forge",
                libsolv::PipAsPythonDependency::No
            );
            REQUIRE(repo_noarch.has_value());

            SECTION("conda-forge/win-64::numpy")
            {
                auto request = Request{
                    /* .flags= */ {},
                    /* .jobs= */ { Request::Install{ "conda-forge/win-64::numpy"_ms } },
                };
                const auto outcome = libsolv::Solver().solve(db, request);

                REQUIRE(outcome.has_value());
                REQUIRE(std::holds_alternative<libsolv::UnSolvable>(outcome.value()));
            }

            SECTION("conda-forge::numpy[subdir=linux-64]")
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
                REQUIRE(actions.size() == 1);
                REQUIRE(std::holds_alternative<Solution::Install>(actions.front()));
                REQUIRE(util::contains(
                    std::get<Solution::Install>(actions.front()).install.package_url,
                    "linux-64"
                ));
            }
        }
    }

    TEST_CASE("Respect pins")
    {
        using PackageInfo = specs::PackageInfo;

        auto db = libsolv::Database({});

        SECTION("Respect pins through direct dependencies")
        {
            auto pkg1 = PackageInfo("foo");
            pkg1.version = "1.0";
            auto pkg2 = PackageInfo("foo");
            pkg2.version = "2.0";

            db.add_repo_from_packages(std::array{ pkg1, pkg2 });

            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Pin{ "foo=1.0"_ms }, Request::Install{ "foo"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto foo_actions = find_actions_with_name(solution, "foo");
            REQUIRE(foo_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(foo_actions.front()));
            REQUIRE(std::get<Solution::Install>(foo_actions.front()).install.version == "1.0");
        }

        SECTION("Respect pins through indirect dependencies")
        {
            auto pkg1 = PackageInfo("foo");
            pkg1.version = "1.0";
            auto pkg2 = PackageInfo("foo");
            pkg2.version = "2.0";
            auto pkg3 = PackageInfo("bar");
            pkg3.version = "1.0";
            pkg3.dependencies = { "foo=1.0" };
            auto pkg4 = PackageInfo("bar");
            pkg4.version = "2.0";
            pkg4.dependencies = { "foo=2.0" };

            db.add_repo_from_packages(std::array{ pkg1, pkg2, pkg3, pkg4 });

            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Pin{ "foo=1.0"_ms }, Request::Install{ "bar"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            const auto foo_actions = find_actions_with_name(solution, "foo");
            REQUIRE(foo_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(foo_actions.front()));
            REQUIRE(std::get<Solution::Install>(foo_actions.front()).install.version == "1.0");

            const auto bar_actions = find_actions_with_name(solution, "bar");
            REQUIRE(bar_actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(bar_actions.front()));
            REQUIRE(std::get<Solution::Install>(bar_actions.front()).install.version == "1.0");
        }
    }

    TEST_CASE("Handle complex matchspecs")
    {
        using PackageInfo = specs::PackageInfo;

        auto db = libsolv::Database({});

        SECTION("*[md5=0bab699354cbd66959550eb9b9866620]")
        {
            auto pkg1 = PackageInfo("foo");
            pkg1.md5 = "0bab699354cbd66959550eb9b9866620";
            auto pkg2 = PackageInfo("foo");
            pkg2.md5 = "bad";

            db.add_repo_from_packages(std::array{ pkg1, pkg2 });

            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "*[md5=0bab699354cbd66959550eb9b9866620]"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE(solution.actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(solution.actions.front()));
            CHECK_EQ(
                std::get<Solution::Install>(solution.actions.front()).install.md5,
                "0bab699354cbd66959550eb9b9866620"
            );
        }

        SECTION("foo[md5=notreallymd5]")
        {
            auto pkg1 = PackageInfo("foo");
            pkg1.md5 = "0bab699354cbd66959550eb9b9866620";

            db.add_repo_from_packages(std::array{ pkg1 });

            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo[md5=notreallymd5]"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<libsolv::UnSolvable>(outcome.value()));
        }

        SECTION("foo[build_string=bld]")
        {
            auto pkg1 = PackageInfo("foo");
            pkg1.build_string = "bad";
            auto pkg2 = PackageInfo("foo");
            pkg2.build_string = "bld";

            db.add_repo_from_packages(std::array{ pkg1, pkg2 });

            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo[build=bld]"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE(solution.actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(solution.actions.front()));
            REQUIRE(
                std::get<Solution::Install>(solution.actions.front()).install.build_string == "bld"
            );
        }

        SECTION("foo[build_string=bld, build_number='>2']")
        {
            auto pkg1 = PackageInfo("foo");
            pkg1.build_string = "bad";
            pkg1.build_number = 3;
            auto pkg2 = PackageInfo("foo");
            pkg2.build_string = "bld";
            pkg2.build_number = 2;
            auto pkg3 = PackageInfo("foo");
            pkg3.build_string = "bld";
            pkg3.build_number = 4;

            db.add_repo_from_packages(std::array{ pkg1, pkg2, pkg3 });

            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo[build=bld]"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(outcome.value()));
            const auto& solution = std::get<Solution>(outcome.value());

            REQUIRE(solution.actions.size() == 1);
            REQUIRE(std::holds_alternative<Solution::Install>(solution.actions.front()));
            REQUIRE(
                std::get<Solution::Install>(solution.actions.front()).install.build_string == "bld"
            );
            REQUIRE(std::get<Solution::Install>(solution.actions.front()).install.build_number == 4);
        }

        SECTION("foo[version='=*,=*', build='pyhd*']")
        {
            auto pkg = PackageInfo("foo");
            pkg.version = "=*,=*";
            pkg.build_string = "pyhd*";

            db.add_repo_from_packages(std::array{ pkg });

            auto request = Request{
                /* .flags= */ {},
                /* .jobs= */ { Request::Install{ "foo[version='=*,=*', build='pyhd*']"_ms } },
            };
            const auto outcome = libsolv::Solver().solve(db, request);

            REQUIRE(outcome.has_value());
            REQUIRE(std::holds_alternative<libsolv::UnSolvable>(outcome.value()));

            const auto& unsolvable = std::get<libsolv::UnSolvable>(outcome.value());
            const auto problems_explained = unsolvable.explain_problems(db, {});
            // To avoid mismatch due to color formatting, we perform the check by splitting the
            // output following the format
            REQUIRE(util::contains(problems_explained, "foo =*,=* pyhd*"));
            REQUIRE(util::contains(
                problems_explained,
                "does not exist (perhaps a typo or a missing channel)."
            ));
        }
    }
}
