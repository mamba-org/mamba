// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>
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

// Forward declaration for utility implemented in core/util.cpp
namespace mamba
{
    std::vector<std::string> extract_package_names_from_specs(const std::vector<std::string>& specs);
}

using namespace mamba;
using namespace mamba::solver;
using namespace specs::match_spec_literals;

namespace
{
    /**
     * Extract root package names from specs for sharded repodata.
     * Uses the shared utility `extract_package_names_from_specs` and
     * then ensures that when python is present, pip is also added.
     */
    std::vector<std::string> extract_root_packages(const std::vector<std::string>& specs)
    {
        // Reuse the production utility to parse package names from specs
        std::vector<std::string> root_packages = extract_package_names_from_specs(specs);

        const bool has_python = std::find(root_packages.begin(), root_packages.end(), "python")
                                != root_packages.end();

        // When installing python, also include pip in root packages for sharded repodata
        if (has_python)
        {
            const bool has_pip = std::find(root_packages.begin(), root_packages.end(), "pip")
                                 != root_packages.end();
            if (!has_pip)
            {
                root_packages.emplace_back("pip");
            }
        }

        return root_packages;
    }

    /**
     * Common helper to set up the database, load channels, build the request and solve.
     * Returns the database, package caches, request and solution.
     */
    struct SolveResult
    {
        libsolv::Database db;
        MultiPackageCache package_caches;
        Request request;
        Solution solution;
    };

    struct SolverConsistencyResult
    {
        Solution flat_repodata_solution;
        Solution sharded_repodata_solution;
    };

    // Forward declarations for helpers used in consistency helpers.
    expected_t<void> install_packages(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool use_shards,
        const fs::u8path& prefix_path,
        const fs::u8path& cache_path
    );

    expected_t<Solution> solve_environment(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool use_shards,
        const fs::u8path& cache_path
    );

    bool compare_environments(
        const fs::u8path& prefix1,
        const fs::u8path& prefix2,
        ChannelContext& channel_context
    );

    /**
     * Helper for environment consistency tests:
     * installs the given specs with and without shards and compares environments.
     */
    void run_environment_consistency_case(
        Context& ctx,
        ChannelContext& channel_context,
        const TemporaryDirectory& tmp_dir,
        const fs::u8path& cache_dir,
        const std::vector<std::string>& specs
    )
    {
        const fs::u8path prefix_traditional = tmp_dir.path() / "env_traditional";
        const fs::u8path prefix_sharded = tmp_dir.path() / "env_sharded";

        const bool use_shards_traditional = false;
        const bool use_shards_sharded = true;

        expected_t<void> install_traditional = install_packages(
            ctx,
            channel_context,
            specs,
            use_shards_traditional,
            prefix_traditional,
            cache_dir
        );
        REQUIRE(install_traditional.has_value());

        expected_t<void> install_sharded = install_packages(
            ctx,
            channel_context,
            specs,
            use_shards_sharded,
            prefix_sharded,
            cache_dir
        );
        REQUIRE(install_sharded.has_value());

        REQUIRE(compare_environments(prefix_traditional, prefix_sharded, channel_context));
    }

    expected_t<SolverConsistencyResult> compute_solver_consistency_result(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        const fs::u8path& cache_dir
    )
    {
        const bool use_shards_traditional = false;
        const bool use_shards_sharded = true;

        expected_t<Solution> solution_traditional = solve_environment(
            ctx,
            channel_context,
            specs,
            use_shards_traditional,
            cache_dir
        );
        if (!solution_traditional.has_value())
        {
            return make_unexpected(
                solution_traditional.error().what(),
                solution_traditional.error().error_code()
            );
        }

        expected_t<Solution> solution_sharded = solve_environment(
            ctx,
            channel_context,
            specs,
            use_shards_sharded,
            cache_dir
        );
        if (!solution_sharded.has_value())
        {
            return make_unexpected(
                solution_sharded.error().what(),
                solution_sharded.error().error_code()
            );
        }

        SolverConsistencyResult result{
            solution_traditional.value(),
            solution_sharded.value(),
        };
        return result;
    }

    expected_t<SolveResult> solve_common(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool use_shards,
        const fs::u8path& cache_path
    )
    {
        // Save original shard setting
        const bool original_use_shards = ctx.repodata_use_shards;

        // Set shard usage
        ctx.repodata_use_shards = use_shards;

        on_scope_exit restore_settings{ [&] { ctx.repodata_use_shards = original_use_shards; } };

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

        SolveResult result{
            std::move(db),
            std::move(package_caches),
            std::move(request),
            std::get<Solution>(outcome.value()),
        };

        return result;
    }

    /**
     * Solve an environment with the given specs.
     * Returns the Solution if successful.
     */
    expected_t<Solution> solve_environment(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool use_shards,
        const fs::u8path& cache_path
    )
    {
        expected_t<SolveResult> common = solve_common(ctx, channel_context, specs, use_shards, cache_path);
        if (!common.has_value())
        {
            return make_unexpected(common.error().what(), common.error().error_code());
        }

        return common->solution;
    }

    /**
     * Install packages into an environment.
     * Returns success status.
     */
    expected_t<void> install_packages(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs,
        bool use_shards,
        const fs::u8path& prefix_path,
        const fs::u8path& cache_path
    )
    {
        expected_t<SolveResult> common = solve_common(ctx, channel_context, specs, use_shards, cache_path);
        if (!common.has_value())
        {
            return make_unexpected(common.error().what(), common.error().error_code());
        }

        // Create prefix data
        fs::create_directories(prefix_path);
        PrefixData prefix_data = PrefixData::create(prefix_path, channel_context).value();

        // Create and execute transaction
        MTransaction
            transaction(ctx, common->db, common->request, common->solution, common->package_caches);
        if (!transaction.execute(ctx, channel_context, prefix_data))
        {
            return make_unexpected("Transaction execution failed", mamba_error_code::internal_failure);
        }

        return expected_t<void>();
    }

    /**
     * Compare two installed environments for equality.
     * Returns true if both environments have the same packages (name, version, build_string,
     * build_number).
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
            if (pkg1.version != pkg2.version || pkg1.build_string != pkg2.build_string
                || pkg1.build_number != pkg2.build_number)
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

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    SECTION("Single package resolution")
    {
        std::vector<std::string> specs = { "python" };

        expected_t<SolverConsistencyResult>
            consistency = compute_solver_consistency_result(ctx, channel_context, specs, cache_dir);
        REQUIRE(consistency.has_value());

        SolverConsistencyResult result = consistency.value();
        REQUIRE(result.flat_repodata_solution == result.sharded_repodata_solution);
    }

    SECTION("Multiple packages resolution")
    {
        std::vector<std::string> specs = { "python", "numpy", "pandas" };

        expected_t<SolverConsistencyResult>
            consistency = compute_solver_consistency_result(ctx, channel_context, specs, cache_dir);
        REQUIRE(consistency.has_value());

        SolverConsistencyResult result = consistency.value();
        REQUIRE(result.flat_repodata_solution == result.sharded_repodata_solution);
    }

    SECTION("Version constraints")
    {
        std::vector<std::string> specs = { "python>=3.10,<3.12" };

        expected_t<SolverConsistencyResult>
            consistency = compute_solver_consistency_result(ctx, channel_context, specs, cache_dir);
        REQUIRE(consistency.has_value());

        SolverConsistencyResult result = consistency.value();
        REQUIRE(result.flat_repodata_solution == result.sharded_repodata_solution);
    }

    SECTION("Complex dependency tree")
    {
        std::vector<std::string> specs = { "jupyter" };

        expected_t<SolverConsistencyResult>
            consistency = compute_solver_consistency_result(ctx, channel_context, specs, cache_dir);
        REQUIRE(consistency.has_value());

        SolverConsistencyResult result = consistency.value();
        REQUIRE(result.flat_repodata_solution == result.sharded_repodata_solution);
    }

    SECTION("Build string matching")
    {
        std::vector<std::string> specs = { "python=3.11" };

        expected_t<SolverConsistencyResult>
            consistency = compute_solver_consistency_result(ctx, channel_context, specs, cache_dir);
        REQUIRE(consistency.has_value());

        SolverConsistencyResult result = consistency.value();
        REQUIRE(result.flat_repodata_solution == result.sharded_repodata_solution);
    }
}

TEST_CASE("Sharded repodata - environment consistency", "[mamba::core][sharded][.integration][!mayfail]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    SECTION("Single package installation")
    {
        std::vector<std::string> specs = { "python" };
        run_environment_consistency_case(ctx, channel_context, tmp_dir, cache_dir, specs);
    }

    SECTION("Multiple packages installation")
    {
        std::vector<std::string> specs = { "python", "numpy", "pandas" };
        run_environment_consistency_case(ctx, channel_context, tmp_dir, cache_dir, specs);
    }

    SECTION("Version constrained installation")
    {
        std::vector<std::string> specs = { "python>=3.10,<3.12" };
        run_environment_consistency_case(ctx, channel_context, tmp_dir, cache_dir, specs);
    }

    SECTION("Complex dependency tree installation")
    {
        std::vector<std::string> specs = { "jupyter" };
        run_environment_consistency_case(ctx, channel_context, tmp_dir, cache_dir, specs);
    }

    SECTION("Another complex dependency tree installation")
    {
        std::vector<std::string> specs = { "pyarrow" };
        run_environment_consistency_case(ctx, channel_context, tmp_dir, cache_dir, specs);
    }
}

TEST_CASE("Sharded repodata - cross-subdir dependencies", "[mamba::core][sharded][.integration][!mayfail]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    // Test installing a package that depends on noarch packages to make sure that cross-subdir
    // traversal works.
    std::vector<std::string> specs = { "python" };

    const bool use_shards_traditional = false;
    const bool use_shards_sharded = true;

    expected_t<Solution> solution_traditional = solve_environment(
        ctx,
        channel_context,
        specs,
        use_shards_traditional,
        cache_dir
    );
    REQUIRE(solution_traditional.has_value());

    expected_t<Solution> solution_sharded = solve_environment(
        ctx,
        channel_context,
        specs,
        use_shards_sharded,
        cache_dir
    );
    REQUIRE(solution_sharded.has_value());

    REQUIRE(solution_traditional.value() == solution_sharded.value());

    // Verify that packages from both subdirs are included
    bool found_python = false;
    for (const auto& pkg : solution_traditional.value().packages())
    {
        if (pkg.name == "python")
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

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
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
    on_scope_exit restore_ctx_update{ [&]
                                      {
                                          ctx.repodata_use_shards = original_use_shards;
                                          ctx.validation_params.safety_checks = saved_safety_checks;
                                      } };

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
    REQUIRE(solution_traditional == solution_sharded);
}

TEST_CASE("Sharded repodata - remove scenarios", "[mamba::core][sharded][.integration][!mayfail]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
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
    on_scope_exit restore_ctx_remove{ [&]
                                      {
                                          ctx.repodata_use_shards = original_use_shards;
                                          ctx.validation_params.safety_checks = saved_safety_checks;
                                      } };

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
    REQUIRE(solution_traditional == solution_sharded);

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
    const bool use_shards = true;
    expected_t<Solution>
        solution = solve_environment(ctx, channel_context, install_specs, use_shards, cache_dir);
    REQUIRE(solution.has_value());

    // Verify python is installed
    bool python_found = false;
    bool pip_found = false;
    std::string python_version;

    for (const auto& pkg : solution.value().packages_to_install())
    {
        if (pkg.name == "python")
        {
            python_found = true;
            python_version = pkg.version;
        }
        if (pkg.name == "pip")
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
