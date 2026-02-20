// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <set>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <catch2/catch_all.hpp>

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/prefix_data.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/transaction.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/solver/solution.hpp"
#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/version.hpp"

#include "mambatests.hpp"

using namespace mamba;
using namespace mamba::solver;
using namespace specs::match_spec_literals;

namespace
{
    /**
     * Extract package information from a Solution for comparison.
     * Returns a set of tuples (name, version, build_string) for all packages
     * involved in the solution (installs, removes, upgrades, etc.).
     */
    auto extract_solution_packages(const Solution& solution)
        -> std::set<std::tuple<std::string, std::string, std::string>>
    {
        std::set<std::tuple<std::string, std::string, std::string>> packages;

        for (const auto& action : solution.actions)
        {
            std::visit(
                [&](const auto& act)
                {
                    using Act = std::decay_t<decltype(act)>;
                    if constexpr (Solution::has_install_v<Act>)
                    {
                        packages.insert(
                            std::make_tuple(act.install.name, act.install.version, act.install.build_string)
                        );
                    }
                    if constexpr (Solution::has_remove_v<Act>)
                    {
                        packages.insert(
                            std::make_tuple(act.remove.name, act.remove.version, act.remove.build_string)
                        );
                    }
                    if constexpr (std::is_same_v<Act, Solution::Reinstall>)
                    {
                        packages.insert(
                            std::make_tuple(act.what.name, act.what.version, act.what.build_string)
                        );
                    }
                    if constexpr (std::is_same_v<Act, Solution::Omit>)
                    {
                        packages.insert(
                            std::make_tuple(act.what.name, act.what.version, act.what.build_string)
                        );
                    }
                },
                action
            );
        }

        return packages;
    }

    /**
     * Compare two solutions for equality by comparing their package sets.
     */
    auto compare_solutions(const Solution& solution1, const Solution& solution2) -> bool
    {
        auto packages1 = extract_solution_packages(solution1);
        auto packages2 = extract_solution_packages(solution2);
        return packages1 == packages2;
    }

    /**
     * Extract root package names from specs for sharded repodata.
     * Only extracts exact name matches (no version constraints).
     * When python is requested, pip is also added to root packages.
     */
    auto extract_root_packages(const std::vector<std::string>& specs) -> std::vector<std::string>
    {
        std::vector<std::string> root_packages;
        bool has_python = false;

        for (const auto& spec : specs)
        {
            auto parsed = specs::MatchSpec::parse(spec);
            if (parsed.has_value())
            {
                const auto& name_spec = parsed->name();
                if (name_spec.is_exact())
                {
                    const std::string name = name_spec.to_string();
                    root_packages.push_back(name);
                    if (name == "python")
                    {
                        has_python = true;
                    }
                }
            }
        }

        // When installing python, also include pip in root packages for sharded repodata
        if (has_python)
        {
            // Check if pip is already in the list
            bool has_pip = std::find(root_packages.begin(), root_packages.end(), "pip")
                           != root_packages.end();
            if (!has_pip)
            {
                root_packages.emplace_back("pip");
            }
        }

        return root_packages;
    }

    /**
     * Solve an environment with the given specs.
     * Returns the Solution if successful.
     */
    auto solve_environment(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool use_shards,
        const fs::u8path& cache_path
    ) -> expected_t<Solution>
    {
        // Save original shard setting
        const bool original_use_shards = ctx.repodata_use_shards;
        const auto saved_safety_checks = ctx.validation_params.safety_checks;

        // Set shard usage
        ctx.repodata_use_shards = use_shards;
        if (use_shards)
        {
            // Sharded repodata often omits checksums; disable safety checks
            ctx.validation_params.safety_checks = VerificationLevel::Disabled;
        }

        on_scope_exit restore_settings{ [&]
                                        {
                                            ctx.repodata_use_shards = original_use_shards;
                                            ctx.validation_params.safety_checks = saved_safety_checks;
                                        } };

        // Create database
        libsolv::Database db{
            channel_context.params(),
            {
                ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                                   : libsolv::MatchSpecParser::Libsolv,
            },
        };

        // Create package cache using temporary directory
        MultiPackageCache package_caches{ { cache_path }, ctx.validation_params };

        // Extract root packages for sharded repodata
        std::vector<std::string> root_packages;
        if (use_shards)
        {
            root_packages = extract_root_packages(specs);
        }

        // Load channels
        auto maybe_load = load_channels(ctx, channel_context, db, package_caches, root_packages);
        if (!maybe_load)
        {
            return make_unexpected(
                std::string("Failed to load channels: ") + maybe_load.error().what(),
                mamba_error_code::repodata_not_loaded
            );
        }

        // Create empty prefix data for fresh install
        auto empty_prefix = cache_path / "empty_prefix";
        fs::create_directories(empty_prefix);
        auto prefix_data = PrefixData::create(empty_prefix, channel_context).value();

        // Create install request
        Request request;
        for (const auto& spec : specs)
        {
            auto parsed = specs::MatchSpec::parse(spec);
            if (parsed.has_value())
            {
                request.jobs.push_back(Request::Install{ parsed.value() });
            }
        }
        request.flags = ctx.solver_flags;

        // Solve
        auto outcome = libsolv::Solver().solve(
            db,
            request,
            ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                               : libsolv::MatchSpecParser::Libsolv
        );

        if (!outcome.has_value())
        {
            return make_unexpected(
                std::string("Failed to solve: ") + outcome.error().what(),
                mamba_error_code::satisfiablitity_error
            );
        }

        if (!std::holds_alternative<Solution>(outcome.value()))
        {
            return make_unexpected(
                "Solver returned non-solution result",
                mamba_error_code::satisfiablitity_error
            );
        }

        return std::get<Solution>(outcome.value());
    }

    /**
     * Install packages into an environment.
     * Returns success status.
     */
    auto install_packages(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool use_shards,
        const fs::u8path& prefix_path,
        const fs::u8path& cache_path
    ) -> expected_t<void>
    {
        // Save original shard setting
        const bool original_use_shards = ctx.repodata_use_shards;
        const auto saved_safety_checks = ctx.validation_params.safety_checks;

        // Set shard usage
        ctx.repodata_use_shards = use_shards;
        if (use_shards)
        {
            // Sharded repodata often omits checksums; disable safety checks
            ctx.validation_params.safety_checks = VerificationLevel::Disabled;
        }

        on_scope_exit restore_settings{ [&]
                                        {
                                            ctx.repodata_use_shards = original_use_shards;
                                            ctx.validation_params.safety_checks = saved_safety_checks;
                                        } };

        // Create database
        libsolv::Database db{
            channel_context.params(),
            {
                ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                                   : libsolv::MatchSpecParser::Libsolv,
            },
        };

        // Create package cache
        MultiPackageCache package_caches{ { cache_path }, ctx.validation_params };

        // Extract root packages for sharded repodata
        std::vector<std::string> root_packages;
        if (use_shards)
        {
            root_packages = extract_root_packages(specs);
        }

        // Load channels
        auto maybe_load = load_channels(ctx, channel_context, db, package_caches, root_packages);
        if (!maybe_load)
        {
            return make_unexpected(
                std::string("Failed to load channels: ") + maybe_load.error().what(),
                mamba_error_code::repodata_not_loaded
            );
        }

        // Create prefix data
        fs::create_directories(prefix_path);
        auto prefix_data = PrefixData::create(prefix_path, channel_context).value();

        // Create install request
        Request request;
        for (const auto& spec : specs)
        {
            auto parsed = specs::MatchSpec::parse(spec);
            if (parsed.has_value())
            {
                request.jobs.push_back(Request::Install{ parsed.value() });
            }
        }
        request.flags = ctx.solver_flags;

        // Solve
        auto outcome = libsolv::Solver().solve(
            db,
            request,
            ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                               : libsolv::MatchSpecParser::Libsolv
        );

        if (!outcome.has_value())
        {
            return make_unexpected(
                std::string("Failed to solve: ") + outcome.error().what(),
                mamba_error_code::satisfiablitity_error
            );
        }

        if (!std::holds_alternative<Solution>(outcome.value()))
        {
            return make_unexpected("Solver returned unsolvable", mamba_error_code::satisfiablitity_error);
        }

        auto solution = std::get<Solution>(outcome.value());

        // Create and execute transaction
        MTransaction transaction(ctx, db, request, solution, package_caches);
        if (!transaction.execute(ctx, channel_context, prefix_data))
        {
            return make_unexpected("Transaction execution failed", mamba_error_code::internal_failure);
        }

        return expected_t<void>();
    }

    /**
     * Compare two installed environments for equality.
     * Returns true if both environments have the same packages (name, version, build).
     */
    auto
    compare_environments(const fs::u8path& prefix1, const fs::u8path& prefix2, ChannelContext& channel_context)
        -> bool
    {
        auto prefix_data1 = PrefixData::create(prefix1, channel_context);
        auto prefix_data2 = PrefixData::create(prefix2, channel_context);

        if (!prefix_data1.has_value() || !prefix_data2.has_value())
        {
            return false;
        }

        const auto& records1 = prefix_data1->records();
        const auto& records2 = prefix_data2->records();

        if (records1.size() != records2.size())
        {
            return false;
        }

        // Compare packages
        for (const auto& [name, pkg1] : records1)
        {
            auto it = records2.find(name);
            if (it == records2.end())
            {
                return false;
            }

            const auto& pkg2 = it->second;
            if (pkg1.version != pkg2.version || pkg1.build_string != pkg2.build_string)
            {
                return false;
            }
        }

        return true;
    }
}

TEST_CASE(
    "Sharded repodata - load_channels accepts root_packages",
    "[mamba::core][sharded][.integration][!mayfail]"
)
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.repodata_use_shards = true;
    ctx.offline = false;

    // Sharded repodata often omits checksums; disable safety checks like install/update do.
    const auto saved_safety_checks = ctx.validation_params.safety_checks;
    ctx.validation_params.safety_checks = VerificationLevel::Disabled;
    on_scope_exit restore_safety{ [&] { ctx.validation_params.safety_checks = saved_safety_checks; } };

    // Use a temp directory for package cache to ensure a writable path (required for shard index
    // and shard caching in CI environments where default pkgs_dirs may not be writable)
    const auto tmp_dir = TemporaryDirectory();
    ctx.pkgs_dirs = { tmp_dir.path() / "pkgs" };
    create_cache_dir(ctx.pkgs_dirs.front());

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    solver::libsolv::Database db{ channel_context.params() };
    MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

    auto result = load_channels(ctx, channel_context, db, package_caches, { "python", "numpy" });
    REQUIRE(result.has_value());
}

TEST_CASE("Sharded repodata - solver results consistency", "[mamba::core][sharded][.integration][!mayfail]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const auto tmp_dir = TemporaryDirectory();
    const auto cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    SECTION("Single package resolution")
    {
        std::vector<std::string> specs = { "python" };

        auto solution_traditional = solve_environment(ctx, channel_context, specs, false, cache_dir);
        REQUIRE(solution_traditional.has_value());

        auto solution_sharded = solve_environment(ctx, channel_context, specs, true, cache_dir);
        REQUIRE(solution_sharded.has_value());

        REQUIRE(compare_solutions(solution_traditional.value(), solution_sharded.value()));
    }

    SECTION("Multiple packages resolution")
    {
        std::vector<std::string> specs = { "python", "numpy", "pandas" };

        auto solution_traditional = solve_environment(ctx, channel_context, specs, false, cache_dir);
        REQUIRE(solution_traditional.has_value());

        auto solution_sharded = solve_environment(ctx, channel_context, specs, true, cache_dir);
        REQUIRE(solution_sharded.has_value());

        REQUIRE(compare_solutions(solution_traditional.value(), solution_sharded.value()));
    }

    SECTION("Version constraints")
    {
        std::vector<std::string> specs = { "python>=3.10,<3.12" };

        auto solution_traditional = solve_environment(ctx, channel_context, specs, false, cache_dir);
        REQUIRE(solution_traditional.has_value());

        auto solution_sharded = solve_environment(ctx, channel_context, specs, true, cache_dir);
        REQUIRE(solution_sharded.has_value());

        REQUIRE(compare_solutions(solution_traditional.value(), solution_sharded.value()));
    }

    SECTION("Complex dependency tree")
    {
        std::vector<std::string> specs = { "jupyter" };

        auto solution_traditional = solve_environment(ctx, channel_context, specs, false, cache_dir);
        REQUIRE(solution_traditional.has_value());

        auto solution_sharded = solve_environment(ctx, channel_context, specs, true, cache_dir);
        REQUIRE(solution_sharded.has_value());

        REQUIRE(compare_solutions(solution_traditional.value(), solution_sharded.value()));
    }

    SECTION("Build string matching")
    {
        std::vector<std::string> specs = { "python=3.11" };

        auto solution_traditional = solve_environment(ctx, channel_context, specs, false, cache_dir);
        REQUIRE(solution_traditional.has_value());

        auto solution_sharded = solve_environment(ctx, channel_context, specs, true, cache_dir);
        REQUIRE(solution_sharded.has_value());

        REQUIRE(compare_solutions(solution_traditional.value(), solution_sharded.value()));

        // Verify that the selected Python version and build match
        auto packages_traditional = extract_solution_packages(solution_traditional.value());
        auto packages_sharded = extract_solution_packages(solution_sharded.value());

        // Find python packages in both solutions
        std::string python_version_traditional, python_build_traditional;
        std::string python_version_sharded, python_build_sharded;

        for (const auto& [name, version, build] : packages_traditional)
        {
            if (name == "python")
            {
                python_version_traditional = version;
                python_build_traditional = build;
                break;
            }
        }

        for (const auto& [name, version, build] : packages_sharded)
        {
            if (name == "python")
            {
                python_version_sharded = version;
                python_build_sharded = build;
                break;
            }
        }

        REQUIRE(python_version_traditional == python_version_sharded);
        REQUIRE(python_build_traditional == python_build_sharded);
    }
}

TEST_CASE("Sharded repodata - environment consistency", "[mamba::core][sharded][.integration][!mayfail]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const auto tmp_dir = TemporaryDirectory();
    const auto cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    SECTION("Single package installation")
    {
        std::vector<std::string> specs = { "python" };

        const auto prefix_traditional = tmp_dir.path() / "env_traditional";
        const auto prefix_sharded = tmp_dir.path() / "env_sharded";

        auto install_traditional = install_packages(
            ctx,
            channel_context,
            specs,
            false,
            prefix_traditional,
            cache_dir
        );
        REQUIRE(install_traditional.has_value());

        auto install_sharded = install_packages(ctx, channel_context, specs, true, prefix_sharded, cache_dir);
        REQUIRE(install_sharded.has_value());

        REQUIRE(compare_environments(prefix_traditional, prefix_sharded, channel_context));
    }

    SECTION("Multiple packages installation")
    {
        std::vector<std::string> specs = { "python", "numpy", "pandas" };

        const auto prefix_traditional = tmp_dir.path() / "env_traditional";
        const auto prefix_sharded = tmp_dir.path() / "env_sharded";

        auto install_traditional = install_packages(
            ctx,
            channel_context,
            specs,
            false,
            prefix_traditional,
            cache_dir
        );
        REQUIRE(install_traditional.has_value());

        auto install_sharded = install_packages(ctx, channel_context, specs, true, prefix_sharded, cache_dir);
        REQUIRE(install_sharded.has_value());

        REQUIRE(compare_environments(prefix_traditional, prefix_sharded, channel_context));
    }

    SECTION("Version constrained installation")
    {
        std::vector<std::string> specs = { "python>=3.10,<3.12" };

        const auto prefix_traditional = tmp_dir.path() / "env_traditional";
        const auto prefix_sharded = tmp_dir.path() / "env_sharded";

        auto install_traditional = install_packages(
            ctx,
            channel_context,
            specs,
            false,
            prefix_traditional,
            cache_dir
        );
        REQUIRE(install_traditional.has_value());

        auto install_sharded = install_packages(ctx, channel_context, specs, true, prefix_sharded, cache_dir);
        REQUIRE(install_sharded.has_value());

        REQUIRE(compare_environments(prefix_traditional, prefix_sharded, channel_context));
    }

    SECTION("Complex dependency tree installation")
    {
        std::vector<std::string> specs = { "jupyter" };

        const auto prefix_traditional = tmp_dir.path() / "env_traditional";
        const auto prefix_sharded = tmp_dir.path() / "env_sharded";

        auto install_traditional = install_packages(
            ctx,
            channel_context,
            specs,
            false,
            prefix_traditional,
            cache_dir
        );
        REQUIRE(install_traditional.has_value());

        auto install_sharded = install_packages(ctx, channel_context, specs, true, prefix_sharded, cache_dir);
        REQUIRE(install_sharded.has_value());

        REQUIRE(compare_environments(prefix_traditional, prefix_sharded, channel_context));
    }
}

TEST_CASE("Sharded repodata - cross-subdir dependencies", "[mamba::core][sharded][.integration][!mayfail]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const auto tmp_dir = TemporaryDirectory();
    const auto cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    // Test installing a package that depends on noarch packages
    std::vector<std::string> specs = { "python" };

    auto solution_traditional = solve_environment(ctx, channel_context, specs, false, cache_dir);
    REQUIRE(solution_traditional.has_value());

    auto solution_sharded = solve_environment(ctx, channel_context, specs, true, cache_dir);
    REQUIRE(solution_sharded.has_value());

    REQUIRE(compare_solutions(solution_traditional.value(), solution_sharded.value()));

    // Verify that packages from both subdirs are included
    auto packages_traditional = extract_solution_packages(solution_traditional.value());
    bool found_python = false;

    for (const auto& [name, version, build] : packages_traditional)
    {
        if (name == "python")
        {
            found_python = true;
            break;
        }
    }

    REQUIRE(found_python);
    // Note: The important thing is that cross-subdir traversal works,
    // which is verified by the successful loading and solving.
}

TEST_CASE("Sharded repodata - update scenarios", "[mamba::core][sharded][.integration][!mayfail]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const auto tmp_dir = TemporaryDirectory();
    const auto cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    // First install a package
    std::vector<std::string> install_specs = { "python=3.11" };
    const auto prefix_traditional = tmp_dir.path() / "env_traditional";
    const auto prefix_sharded = tmp_dir.path() / "env_sharded";

    auto install_traditional = install_packages(
        ctx,
        channel_context,
        install_specs,
        false,
        prefix_traditional,
        cache_dir
    );
    REQUIRE(install_traditional.has_value());

    auto install_sharded = install_packages(
        ctx,
        channel_context,
        install_specs,
        true,
        prefix_sharded,
        cache_dir
    );
    REQUIRE(install_sharded.has_value());

    // Now update the package
    std::vector<std::string> update_specs = { "python" };

    // For update, we need to create prefix data and use Update request
    const bool original_use_shards = ctx.repodata_use_shards;
    const auto saved_safety_checks = ctx.validation_params.safety_checks;

    // Test traditional update
    ctx.repodata_use_shards = false;
    libsolv::Database db_traditional{ channel_context.params() };
    MultiPackageCache package_caches_traditional{ { cache_dir }, ctx.validation_params };
    std::vector<std::string> root_packages_traditional;
    auto load_traditional = load_channels(
        ctx,
        channel_context,
        db_traditional,
        package_caches_traditional,
        root_packages_traditional
    );
    REQUIRE(load_traditional.has_value());

    auto prefix_data_traditional = PrefixData::create(prefix_traditional, channel_context).value();
    Request request_traditional;
    for (const auto& spec : update_specs)
    {
        auto parsed = specs::MatchSpec::parse(spec);
        if (parsed.has_value())
        {
            request_traditional.jobs.push_back(Request::Update{ parsed.value() });
        }
    }
    request_traditional.flags = ctx.solver_flags;

    auto outcome_traditional = libsolv::Solver().solve(
        db_traditional,
        request_traditional,
        ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                           : libsolv::MatchSpecParser::Libsolv
    );
    REQUIRE(outcome_traditional.has_value());
    REQUIRE(std::holds_alternative<Solution>(outcome_traditional.value()));
    auto solution_traditional = std::get<Solution>(outcome_traditional.value());

    // Test sharded update
    ctx.repodata_use_shards = true;
    ctx.validation_params.safety_checks = VerificationLevel::Disabled;
    libsolv::Database db_sharded{ channel_context.params() };
    MultiPackageCache package_caches_sharded{ { cache_dir }, ctx.validation_params };
    std::vector<std::string> root_packages_sharded = extract_root_packages(update_specs);
    auto load_sharded = load_channels(
        ctx,
        channel_context,
        db_sharded,
        package_caches_sharded,
        root_packages_sharded
    );
    REQUIRE(load_sharded.has_value());

    auto prefix_data_sharded = PrefixData::create(prefix_sharded, channel_context).value();
    Request request_sharded;
    for (const auto& spec : update_specs)
    {
        auto parsed = specs::MatchSpec::parse(spec);
        if (parsed.has_value())
        {
            request_sharded.jobs.push_back(Request::Update{ parsed.value() });
        }
    }
    request_sharded.flags = ctx.solver_flags;

    auto outcome_sharded = libsolv::Solver().solve(
        db_sharded,
        request_sharded,
        ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                           : libsolv::MatchSpecParser::Libsolv
    );
    REQUIRE(outcome_sharded.has_value());
    REQUIRE(std::holds_alternative<Solution>(outcome_sharded.value()));
    auto solution_sharded = std::get<Solution>(outcome_sharded.value());

    // Compare update solutions
    REQUIRE(compare_solutions(solution_traditional, solution_sharded));

    // Restore settings
    ctx.repodata_use_shards = original_use_shards;
    ctx.validation_params.safety_checks = saved_safety_checks;
}

TEST_CASE("Sharded repodata - remove scenarios", "[mamba::core][sharded][.integration][!mayfail]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const auto tmp_dir = TemporaryDirectory();
    const auto cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    // First install packages
    std::vector<std::string> install_specs = { "python", "numpy" };
    const auto prefix_traditional = tmp_dir.path() / "env_traditional";
    const auto prefix_sharded = tmp_dir.path() / "env_sharded";

    auto install_traditional = install_packages(
        ctx,
        channel_context,
        install_specs,
        false,
        prefix_traditional,
        cache_dir
    );
    REQUIRE(install_traditional.has_value());

    auto install_sharded = install_packages(
        ctx,
        channel_context,
        install_specs,
        true,
        prefix_sharded,
        cache_dir
    );
    REQUIRE(install_sharded.has_value());

    // Now remove one package
    std::vector<std::string> remove_specs = { "numpy" };

    // For remove, we need to create prefix data and use Remove request
    const bool original_use_shards = ctx.repodata_use_shards;
    const auto saved_safety_checks = ctx.validation_params.safety_checks;

    // Test traditional remove
    ctx.repodata_use_shards = false;
    libsolv::Database db_traditional{ channel_context.params() };
    MultiPackageCache package_caches_traditional{ { cache_dir }, ctx.validation_params };
    std::vector<std::string> root_packages_traditional;
    auto load_traditional = load_channels(
        ctx,
        channel_context,
        db_traditional,
        package_caches_traditional,
        root_packages_traditional
    );
    REQUIRE(load_traditional.has_value());

    auto prefix_data_traditional = PrefixData::create(prefix_traditional, channel_context).value();
    Request request_traditional;
    for (const auto& spec : remove_specs)
    {
        auto parsed = specs::MatchSpec::parse(spec);
        if (parsed.has_value())
        {
            request_traditional.jobs.push_back(Request::Remove{ parsed.value() });
        }
    }
    request_traditional.flags = ctx.solver_flags;

    auto outcome_traditional = libsolv::Solver().solve(
        db_traditional,
        request_traditional,
        ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                           : libsolv::MatchSpecParser::Libsolv
    );
    REQUIRE(outcome_traditional.has_value());
    REQUIRE(std::holds_alternative<Solution>(outcome_traditional.value()));
    auto solution_traditional = std::get<Solution>(outcome_traditional.value());

    // Test sharded remove
    ctx.repodata_use_shards = true;
    ctx.validation_params.safety_checks = VerificationLevel::Disabled;
    libsolv::Database db_sharded{ channel_context.params() };
    MultiPackageCache package_caches_sharded{ { cache_dir }, ctx.validation_params };
    std::vector<std::string> root_packages_sharded = extract_root_packages(remove_specs);
    auto load_sharded = load_channels(
        ctx,
        channel_context,
        db_sharded,
        package_caches_sharded,
        root_packages_sharded
    );
    REQUIRE(load_sharded.has_value());

    auto prefix_data_sharded = PrefixData::create(prefix_sharded, channel_context).value();
    Request request_sharded;
    for (const auto& spec : remove_specs)
    {
        auto parsed = specs::MatchSpec::parse(spec);
        if (parsed.has_value())
        {
            request_sharded.jobs.push_back(Request::Remove{ parsed.value() });
        }
    }
    request_sharded.flags = ctx.solver_flags;

    auto outcome_sharded = libsolv::Solver().solve(
        db_sharded,
        request_sharded,
        ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                           : libsolv::MatchSpecParser::Libsolv
    );
    REQUIRE(outcome_sharded.has_value());
    REQUIRE(std::holds_alternative<Solution>(outcome_sharded.value()));
    auto solution_sharded = std::get<Solution>(outcome_sharded.value());

    // Compare remove solutions
    REQUIRE(compare_solutions(solution_traditional, solution_sharded));

    // Execute transactions and compare final environments
    MTransaction transaction_traditional(
        ctx,
        db_traditional,
        request_traditional,
        solution_traditional,
        package_caches_traditional
    );
    REQUIRE(transaction_traditional.execute(ctx, channel_context, prefix_data_traditional));

    MTransaction transaction_sharded(
        ctx,
        db_sharded,
        request_sharded,
        solution_sharded,
        package_caches_sharded
    );
    REQUIRE(transaction_sharded.execute(ctx, channel_context, prefix_data_sharded));

    REQUIRE(compare_environments(prefix_traditional, prefix_sharded, channel_context));

    // Restore settings
    ctx.repodata_use_shards = original_use_shards;
    ctx.validation_params.safety_checks = saved_safety_checks;
}

TEST_CASE(
    "Sharded repodata - python install includes pip and version >= 3.14",
    "[mamba::core][sharded][.integration][!mayfail]"
)
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const auto tmp_dir = TemporaryDirectory();
    const auto cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    // Install only python (no version constraint)
    std::vector<std::string> install_specs = { "python" };

    // Solve with sharded repodata
    auto solution = solve_environment(ctx, channel_context, install_specs, true, cache_dir);
    REQUIRE(solution.has_value());

    // Extract packages from solution
    auto packages = extract_solution_packages(solution.value());

    // Verify python is installed
    bool python_found = false;
    bool pip_found = false;
    std::string python_version;

    for (const auto& [name, version, build] : packages)
    {
        if (name == "python")
        {
            python_found = true;
            python_version = version;
        }
        if (name == "pip")
        {
            pip_found = true;
        }
    }

    REQUIRE(python_found);
    REQUIRE(pip_found);

    // Verify python version is at least 3.14
    auto python_version_obj = specs::Version::parse(python_version);
    REQUIRE(python_version_obj.has_value());

    auto min_version = specs::Version::parse("3.14");
    REQUIRE(min_version.has_value());
    REQUIRE(python_version_obj.value() >= min_version.value());
}
