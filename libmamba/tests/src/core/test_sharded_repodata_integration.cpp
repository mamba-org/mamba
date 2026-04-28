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
#include "mamba/core/history.hpp"
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

// extract_package_names_from_specs is implemented in api/utils.cpp (header is not in public include
// path)
namespace mamba
{
    std::vector<std::string> extract_package_names_from_specs(const std::vector<std::string>& specs);
    void add_python_related_roots_if_python_requested(std::vector<std::string>& root_packages);
    std::pair<solver::libsolv::Database, MultiPackageCache> prepare_solver_context(
        Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& raw_specs,
        bool is_retry,
        bool no_py_pin
    );
    PrefixData load_prefix_data_and_installed(
        Context& ctx,
        ChannelContext& channel_context,
        solver::libsolv::Database& db
    );
}

using namespace mamba;
using namespace mamba::solver;
using namespace specs::match_spec_literals;

namespace
{
    /**
     * Extract root package names from specs for sharded repodata.
     * Uses shared utilities and ensures python-related roots are added.
     */
    std::vector<std::string> extract_root_packages(const std::vector<std::string>& specs)
    {
        std::vector<std::string> root_packages = extract_package_names_from_specs(specs);
        add_python_related_roots_if_python_requested(root_packages);
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
        const bool original_use_shards = ctx.use_sharded_repodata;

        // Set shard usage
        ctx.use_sharded_repodata = use_shards;

        on_scope_exit restore_settings{ [&] { ctx.use_sharded_repodata = original_use_shards; } };

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

TEST_CASE("Sharded repodata - load_channels accepts root_packages", "[mamba::core][sharded][.integration]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.use_sharded_repodata = true;
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

TEST_CASE("Sharded repodata - python root extraction adds python_abi", "[mamba::core][sharded]")
{
    SECTION("python spec adds pip and python_abi roots")
    {
        const auto roots = extract_root_packages({ "python>=3.11", "numpy" });
        REQUIRE(roots == std::vector<std::string>{ "python", "numpy", "pip", "python_abi" });
    }

    SECTION("existing pip/python_abi are not duplicated")
    {
        const auto roots = extract_root_packages({ "python", "pip", "python_abi" });
        REQUIRE(roots == std::vector<std::string>{ "python", "pip", "python_abi" });
    }
}

TEST_CASE("Sharded repodata - noarch-only root package is installable", "[mamba::core][sharded][.integration]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    // `tzdata` is a noarch package with no dependencies; this exercises the edge case where
    // the root package is resolved from a sibling subdir while iterating platform subdirs.
    const std::vector<std::string> specs = { "tzdata" };

    auto flat_solution = solve_environment(ctx, channel_context, specs, false, cache_dir);
    REQUIRE(flat_solution.has_value());

    auto sharded_solution = solve_environment(ctx, channel_context, specs, true, cache_dir);
    REQUIRE(sharded_solution.has_value());
    REQUIRE(flat_solution.value() == sharded_solution.value());

    bool tzdata_found = false;
    for (const auto& pkg : sharded_solution->packages_to_install())
    {
        if (pkg.name == "tzdata")
        {
            tzdata_found = true;
            break;
        }
    }
    REQUIRE(tzdata_found);
}

// Resolves `python` using the real `conda-forge` channel (repo.anaconda.org / conda.anaconda.org)
// with `use_sharded_repodata`: shard index fetch, per-package shard downloads, repodata build, and
// solver. This is the same sharded path `micromamba create` uses before linking; linking is not
// exercised here so the test stays stable in constrained CI environments.
TEST_CASE(
    "Sharded repodata - solve python with conda-forge (anaconda.org)",
    "[mamba::core][sharded][.integration]"
)
{
    auto& ctx = mambatests::context();
    const std::vector<std::string> saved_channels = ctx.channels;
    const bool saved_use_shards = ctx.use_sharded_repodata;
    const bool saved_offline = ctx.offline;
    on_scope_exit restore_ctx{ [&]
                               {
                                   ctx.channels = saved_channels;
                                   ctx.use_sharded_repodata = saved_use_shards;
                                   ctx.offline = saved_offline;
                               } };

    ctx.channels = { "conda-forge" };
    ctx.use_sharded_repodata = true;
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    auto solved = solve_environment(
        ctx,
        channel_context,
        std::vector<std::string>{ "python" },
        true,
        cache_dir
    );
    REQUIRE(solved.has_value());
    bool found_python = false;
    for (const auto& pkg : solved.value().packages())
    {
        if (pkg.name == "python")
        {
            found_python = true;
            break;
        }
    }
    REQUIRE(found_python);
}

// Exercises the same sharded path with a large dependency tree: shard index, per-package shards,
// repodata build, and solver. Ensures packages like tensorflow (many python-version-specific
// builds in shards) remain resolvable when `use_sharded_repodata` is enabled.
TEST_CASE(
    "Sharded repodata - solve tensorflow with conda-forge (anaconda.org)",
    "[mamba::core][sharded][.integration]"
)
{
    auto& ctx = mambatests::context();
    const std::vector<std::string> saved_channels = ctx.channels;
    const bool saved_use_shards = ctx.use_sharded_repodata;
    const bool saved_offline = ctx.offline;
    on_scope_exit restore_ctx{ [&]
                               {
                                   ctx.channels = saved_channels;
                                   ctx.use_sharded_repodata = saved_use_shards;
                                   ctx.offline = saved_offline;
                               } };

    ctx.channels = { "conda-forge" };
    ctx.use_sharded_repodata = true;
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    auto solved = solve_environment(
        ctx,
        channel_context,
        std::vector<std::string>{ "tensorflow" },
        true,
        cache_dir
    );
    REQUIRE(solved.has_value());
    bool found_tensorflow = false;
    for (const auto& pkg : solved.value().packages())
    {
        if (pkg.name == "tensorflow")
        {
            found_tensorflow = true;
            break;
        }
    }
    REQUIRE(found_tensorflow);
}

TEST_CASE("Sharded repodata - solve xeus-python-dev specs on emscripten", "[mamba::core][sharded][.integration]")
{
    auto& ctx = mambatests::context();
    const std::vector<std::string> saved_channels = ctx.channels;
    const std::string saved_platform = ctx.platform;
    const bool saved_use_shards = ctx.use_sharded_repodata;
    const bool saved_offline = ctx.offline;
    on_scope_exit restore_ctx{ [&]
                               {
                                   ctx.channels = saved_channels;
                                   ctx.platform = saved_platform;
                                   ctx.use_sharded_repodata = saved_use_shards;
                                   ctx.offline = saved_offline;
                               } };

    ctx.channels = {
        "https://prefix.dev/emscripten-forge-4x",
        "https://prefix.dev/conda-forge",
    };
    ctx.platform = "emscripten-wasm32";
    ctx.use_sharded_repodata = true;
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    const std::vector<std::string> specs = {
        "python>=3.11", "pybind11", "nlohmann_json", "pybind11_json", "numpy",
        "pytest",       "bzip2",    "sqlite",        "zlib",          "libffi",
        "xtl",          "pyjs",     "xeus",          "xeus-lite",
    };

    auto solved = solve_environment(ctx, channel_context, specs, true, cache_dir);
    REQUIRE(solved.has_value());

    std::unordered_set<std::string> solved_names;
    for (const auto& pkg : solved->packages())
    {
        solved_names.insert(pkg.name);
    }

    const std::vector<std::string> expected = {
        "python", "python_abi", "pybind11", "nlohmann_json", "pybind11_json",
        "numpy",  "pytest",     "bzip2",    "sqlite",        "zlib",
        "libffi", "xtl",        "pyjs",     "xeus",          "xeus-lite",
    };
    for (const auto& name : expected)
    {
        REQUIRE(solved_names.count(name) == 1);
    }
}

TEST_CASE("Sharded repodata - solve pyjs-obspy env specs on emscripten", "[mamba::core][sharded][.integration]")
{
    auto& ctx = mambatests::context();
    const std::vector<std::string> saved_channels = ctx.channels;
    const std::string saved_platform = ctx.platform;
    const bool saved_use_shards = ctx.use_sharded_repodata;
    const bool saved_offline = ctx.offline;
    on_scope_exit restore_ctx{ [&]
                               {
                                   ctx.channels = saved_channels;
                                   ctx.platform = saved_platform;
                                   ctx.use_sharded_repodata = saved_use_shards;
                                   ctx.offline = saved_offline;
                               } };

    ctx.channels = {
        "https://repo.prefix.dev/emscripten-forge-4x",
        "https://repo.prefix.dev/conda-forge",
    };
    ctx.platform = "emscripten-wasm32";
    ctx.use_sharded_repodata = true;
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    // Mirror the exact YAML environment spec from the regression report.
    const std::vector<std::string> specs = { "python", "obspy", "matplotlib", "numpy",
                                             "scipy",  "pyjs",  "requests",   "pyodide-http" };

    auto sharded_solution = solve_environment(ctx, channel_context, specs, true, cache_dir);
    REQUIRE(sharded_solution.has_value());

    std::unordered_set<std::string> solved_names;
    for (const auto& pkg : sharded_solution->packages())
    {
        solved_names.insert(pkg.name);
    }

    // Ensure all requested specs are present in the solved environment.
    for (const auto& name : specs)
    {
        REQUIRE(solved_names.count(name) == 1);
    }

    // Ensure key transitive noarch deps from the original failure path are present.
    REQUIRE(solved_names.count("obspy") == 1);
    REQUIRE(solved_names.count("matplotlib") == 1);
    REQUIRE(solved_names.count("requests") == 1);
    REQUIRE(solved_names.count("cycler") == 1);
    REQUIRE(solved_names.count("decorator") == 1);
    REQUIRE(solved_names.count("brotli-python") == 1);
}

TEST_CASE("Sharded repodata - solver results consistency", "[mamba::core][sharded][.integration]")
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

TEST_CASE("Sharded repodata - environment consistency", "[mamba::core][sharded][.integration]")
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

    SECTION("scikit-learn installation")
    {
        std::vector<std::string> specs = { "scikit-learn" };
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

TEST_CASE("Sharded repodata - cross-subdir dependencies", "[mamba::core][sharded][.integration]")
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

TEST_CASE("Sharded repodata - update scenarios", "[mamba::core][sharded][.integration]")
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
    const bool original_use_shards = ctx.use_sharded_repodata;
    const auto saved_safety_checks = ctx.validation_params.safety_checks;
    on_scope_exit restore_ctx_update{ [&]
                                      {
                                          ctx.use_sharded_repodata = original_use_shards;
                                          ctx.validation_params.safety_checks = saved_safety_checks;
                                      } };

    // Test traditional update
    ctx.use_sharded_repodata = false;
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
    ctx.use_sharded_repodata = true;
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

TEST_CASE("Sharded repodata - update all uses history-expanded roots", "[mamba::core][sharded][.integration]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    const auto prefix_path = tmp_dir.path() / "env_update_all";
    fs::create_directories(prefix_path / "conda-meta");

    History history(prefix_path, channel_context);
    History::UserRequest req;
    req.date = "2026-04-27 00:00:00";
    req.cmd = "update --all";
    req.conda_version = "25.0.0";
    req.update = { "conda-smithy" };
    history.add_entry(req);

    struct UpdateAllSolveResult
    {
        std::vector<specs::PackageInfo> packages;
        bool has_conda_smithy_in_db;
    };

    auto solve_update_all_like_api = [&](bool use_shards) -> UpdateAllSolveResult
    {
        const bool saved_use_shards = ctx.use_sharded_repodata;
        const auto saved_target_prefix = ctx.prefix_params.target_prefix;
        on_scope_exit restore_ctx{ [&]
                                   {
                                       ctx.use_sharded_repodata = saved_use_shards;
                                       ctx.prefix_params.target_prefix = saved_target_prefix;
                                   } };

        ctx.use_sharded_repodata = use_shards;
        ctx.prefix_params.target_prefix = prefix_path;

        auto [db, package_caches] = prepare_solver_context(
            ctx,
            channel_context,
            /*raw_specs=*/{},
            /*is_retry=*/false,
            /*no_py_pin=*/false
        );
        (void) package_caches;

        auto prefix_data = load_prefix_data_and_installed(ctx, channel_context, db);
        (void) prefix_data;

        Request request;
        request.jobs.emplace_back(Request::UpdateAll{ /* .clean_dependencies= */ false });
        request.flags = ctx.solver_flags;

        bool has_conda_smithy_in_db = false;
        db.for_each_package_matching(
            specs::MatchSpec::parse("conda-smithy").value(),
            [&](const specs::PackageInfo& pkg)
            {
                if (pkg.name == "conda-smithy")
                {
                    has_conda_smithy_in_db = true;
                    return util::LoopControl::Break;
                }
                return util::LoopControl::Continue;
            }
        );

        auto outcome = libsolv::Solver().solve(
            db,
            request,
            ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                               : libsolv::MatchSpecParser::Libsolv
        );
        REQUIRE(outcome.has_value());
        REQUIRE(std::holds_alternative<Solution>(outcome.value()));
        auto solution = std::get<Solution>(outcome.value());
        std::vector<specs::PackageInfo> packages;
        for (const auto& pkg : solution.packages())
        {
            packages.push_back(pkg);
        }
        return UpdateAllSolveResult{ std::move(packages), has_conda_smithy_in_db };
    };

    const auto flat = solve_update_all_like_api(/*use_shards=*/false);
    const auto sharded = solve_update_all_like_api(/*use_shards=*/true);

    REQUIRE(flat.has_conda_smithy_in_db);
    REQUIRE(sharded.has_conda_smithy_in_db);
    REQUIRE(flat.packages == sharded.packages);
}

TEST_CASE("Sharded repodata - issue 4240 update-all example parity", "[mamba::core][sharded][.integration]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    const auto prefix_path = tmp_dir.path() / "env_issue_4240";
    fs::create_directories(prefix_path / "conda-meta");

    // Seed history with the exact package set from the issue example so update-all root expansion
    // uses these names in shard mode.
    History history(prefix_path, channel_context);
    History::UserRequest req;
    req.date = "2026-04-27 00:00:00";
    req.cmd = "update --all";
    req.conda_version = "25.0.0";
    req.update = {
        "python=3.12",   "conda-smithy=3.61.1", "conda-forge-pinning=2026.04.23.11.42.25",
        "chardet=5.2.0", "requests=2.33.1",
    };
    history.add_entry(req);

    struct UpdateAllSolveResult
    {
        Solution solution;
        bool has_python_in_db = false;
        bool has_conda_smithy_in_db = false;
        bool has_conda_forge_pinning_in_db = false;
        bool has_chardet_in_db = false;
        bool has_requests_in_db = false;
    };

    auto solve_update_all_like_api = [&](bool use_shards) -> UpdateAllSolveResult
    {
        const bool saved_use_shards = ctx.use_sharded_repodata;
        const auto saved_target_prefix = ctx.prefix_params.target_prefix;
        on_scope_exit restore_ctx{ [&]
                                   {
                                       ctx.use_sharded_repodata = saved_use_shards;
                                       ctx.prefix_params.target_prefix = saved_target_prefix;
                                   } };

        ctx.use_sharded_repodata = use_shards;
        ctx.prefix_params.target_prefix = prefix_path;

        auto [db, package_caches] = prepare_solver_context(
            ctx,
            channel_context,
            /*raw_specs=*/{},
            /*is_retry=*/false,
            /*no_py_pin=*/false
        );
        (void) package_caches;

        auto prefix_data = load_prefix_data_and_installed(ctx, channel_context, db);
        (void) prefix_data;

        Request request;
        request.jobs.emplace_back(Request::UpdateAll{ /* .clean_dependencies= */ false });
        request.flags = ctx.solver_flags;

        auto has_name_in_db = [&](std::string_view name) -> bool
        {
            bool found = false;
            db.for_each_package_matching(
                specs::MatchSpec::parse(std::string(name)).value(),
                [&](const specs::PackageInfo& pkg)
                {
                    if (pkg.name == name)
                    {
                        found = true;
                        return util::LoopControl::Break;
                    }
                    return util::LoopControl::Continue;
                }
            );
            return found;
        };

        auto outcome = libsolv::Solver().solve(
            db,
            request,
            ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                               : libsolv::MatchSpecParser::Libsolv
        );
        REQUIRE(outcome.has_value());
        REQUIRE(std::holds_alternative<Solution>(outcome.value()));
        return {
            std::get<Solution>(outcome.value()), has_name_in_db("python"),
            has_name_in_db("conda-smithy"),      has_name_in_db("conda-forge-pinning"),
            has_name_in_db("chardet"),           has_name_in_db("requests"),
        };
    };

    const auto flat = solve_update_all_like_api(/*use_shards=*/false);
    const auto sharded = solve_update_all_like_api(/*use_shards=*/true);

    // Reproducer package names from the issue comment should be present in the solver universe in
    // both modes.
    REQUIRE(flat.has_python_in_db);
    REQUIRE(flat.has_conda_smithy_in_db);
    REQUIRE(flat.has_conda_forge_pinning_in_db);
    REQUIRE(flat.has_chardet_in_db);
    REQUIRE(flat.has_requests_in_db);
    REQUIRE(sharded.has_python_in_db);
    REQUIRE(sharded.has_conda_smithy_in_db);
    REQUIRE(sharded.has_conda_forge_pinning_in_db);
    REQUIRE(sharded.has_chardet_in_db);
    REQUIRE(sharded.has_requests_in_db);

    // Sharded mode should produce the same update-all solution as flat mode.
    REQUIRE(flat.solution == sharded.solution);
}

TEST_CASE("Sharded repodata - remove scenarios", "[mamba::core][sharded][.integration]")
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
    const bool original_use_shards = ctx.use_sharded_repodata;
    const auto saved_safety_checks = ctx.validation_params.safety_checks;
    on_scope_exit restore_ctx_remove{ [&]
                                      {
                                          ctx.use_sharded_repodata = original_use_shards;
                                          ctx.validation_params.safety_checks = saved_safety_checks;
                                      } };

    // Test traditional remove
    ctx.use_sharded_repodata = false;
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
    ctx.use_sharded_repodata = true;
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
    "[mamba::core][sharded][.integration]"
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

TEST_CASE("Sharded repodata - libblas implementation preference", "[mamba::core][sharded][.integration]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.offline = false;

    const auto tmp_dir = TemporaryDirectory();
    const auto cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    std::vector<std::string> install_specs = { "libblas" };

    const bool use_shards = true;
    expected_t<Solution>
        solution = solve_environment(ctx, channel_context, install_specs, use_shards, cache_dir);
    REQUIRE(solution.has_value());

    bool libblas_found = false;
    for (const auto& pkg : solution.value().packages())
    {
        if (pkg.name == "libblas")
        {
            libblas_found = true;
            // OpenBLAS builds do not carry BLAS track_features, whereas
            // netlib / MKL / BLIS variants do. Ensuring an empty
            // track_features set corresponds to selecting the openblas
            // implementation from the available libblas builds.
            REQUIRE(pkg.track_features.empty());
        }
    }
    REQUIRE(libblas_found);
}

TEST_CASE(
    "Sharded repodata - offline recreate uses cache without network",
    "[mamba::core][sharded][.integration]"
)
{
    Context ctx;
    ctx.channels = { "https://prefix.dev/conda-forge" };
    ctx.use_sharded_repodata = true;

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
    init_channels(ctx, channel_context);

    std::vector<std::string> specs = { "xtensor" };
    const fs::u8path prefix_online = tmp_dir.path() / "env_online";
    const fs::u8path prefix_offline = tmp_dir.path() / "env_offline";

    // First create: online, populates shard index cache and package cache
    ctx.offline = false;
    expected_t<void> install_online = install_packages(
        ctx,
        channel_context,
        specs,
        /* use_shards */ true,
        prefix_online,
        cache_dir
    );
    REQUIRE(install_online.has_value());

    // Second create: offline, must succeed using only cached data (no network requests)
    ctx.offline = true;
    expected_t<void> install_offline = install_packages(
        ctx,
        channel_context,
        specs,
        /* use_shards */ true,
        prefix_offline,
        cache_dir
    );
    REQUIRE(install_offline.has_value());

    // Verify both environments are equivalent (confirms offline used only cached data)
    REQUIRE(compare_environments(prefix_online, prefix_offline, channel_context));
}
