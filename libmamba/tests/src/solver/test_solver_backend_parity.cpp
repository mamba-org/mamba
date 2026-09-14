// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <catch2/catch_all.hpp>

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/core/virtual_packages.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/request.hpp"
#include "mamba/solver/resolvo/database.hpp"
#include "mamba/solver/resolvo/solver.hpp"
#include "mamba/specs/match_spec.hpp"

#include "mambatests.hpp"

namespace mamba
{
    std::vector<std::string> extract_package_names_from_specs(const std::vector<std::string>& specs);
    void add_python_related_roots_if_python_requested(std::vector<std::string>& root_packages);
}

using namespace mamba;
using namespace mamba::solver;

namespace
{
    using PackageKey = std::tuple<std::string, std::string, std::string, std::size_t>;

    auto extract_root_packages(const std::vector<std::string>& specs) -> std::vector<std::string>
    {
        auto roots = extract_package_names_from_specs(specs);
        add_python_related_roots_if_python_requested(roots);
        return roots;
    }

    auto make_request(const std::vector<std::string>& specs, const Request::Flags& flags) -> Request
    {
        Request request;
        request.flags = flags;
        for (const auto& spec : specs)
        {
            auto parsed = specs::MatchSpec::parse(spec);
            REQUIRE(parsed.has_value());
            request.jobs.emplace_back(Request::Install{ parsed.value() });
        }
        return request;
    }

    auto as_key_set(const Solution& solution) -> std::set<PackageKey>
    {
        std::set<PackageKey> out;
        for (const auto& pkg : solution.packages_to_install())
        {
            out.emplace(pkg.name, pkg.version, pkg.build_string, pkg.build_number);
        }
        return out;
    }

    auto make_resolvo_db_from_libsolv(const libsolv::Database& libsolv_db)
        -> mamba::solver::resolvo::Database
    {
        auto resolvo_db = mamba::solver::resolvo::Database(libsolv_db.channel_params());
        const auto all = specs::MatchSpec::parse("*");
        REQUIRE(all.has_value());
        std::vector<specs::PackageInfo> packages;
        const_cast<libsolv::Database&>(libsolv_db)
            .for_each_package_matching(
                all.value(),
                [&](specs::PackageInfo&& pkg)
                {
                    packages.emplace_back(std::move(pkg));
                    return util::LoopControl::Continue;
                }
            );
        resolvo_db.add_repo_from_packages(packages, "all", false);
        return resolvo_db;
    }

    struct CaseData
    {
        std::string name;
        std::vector<std::string> channels;
        std::string platform;
        std::vector<std::string> specs;
    };
}

TEST_CASE("Solver backend parity on sharded specs", "[mamba::solver][parity][.integration]")
{
    auto& ctx = mambatests::context();
    const auto saved_channels = ctx.channels;
    const auto saved_platform = ctx.platform;
    const auto saved_offline = ctx.offline;
    const auto saved_use_shards = ctx.use_sharded_repodata;
    on_scope_exit restore_ctx{ [&]
                               {
                                   ctx.channels = saved_channels;
                                   ctx.platform = saved_platform;
                                   ctx.offline = saved_offline;
                                   ctx.use_sharded_repodata = saved_use_shards;
                               } };

    const TemporaryDirectory tmp_dir;
    const fs::u8path cache_dir = tmp_dir.path() / "cache";
    fs::create_directories(cache_dir);
    ctx.pkgs_dirs = { cache_dir };
    create_cache_dir(cache_dir);

    const std::vector<CaseData> cases = {
        { "python", { "https://prefix.dev/conda-forge" }, "linux-64", { "python" } },
        { "python-numpy-pandas",
          { "https://prefix.dev/conda-forge" },
          "linux-64",
          { "python", "numpy", "pandas" } },
        { "python-range", { "https://prefix.dev/conda-forge" }, "linux-64", { "python>=3.10,<3.12" } },
        { "jupyter", { "https://prefix.dev/conda-forge" }, "linux-64", { "jupyter" } },
        { "python-pin", { "https://prefix.dev/conda-forge" }, "linux-64", { "python=3.11" } },
        { "scikit-learn", { "https://prefix.dev/conda-forge" }, "linux-64", { "scikit-learn" } },
        { "pyarrow", { "https://prefix.dev/conda-forge" }, "linux-64", { "pyarrow" } },
        { "tensorflow", { "conda-forge" }, "linux-64", { "tensorflow" } },
        { "emscripten-xeus",
          { "https://prefix.dev/emscripten-forge-4x", "https://prefix.dev/conda-forge" },
          "emscripten-wasm32",
          { "python>=3.11",
            "pybind11",
            "nlohmann_json",
            "pybind11_json",
            "numpy",
            "pytest",
            "bzip2",
            "sqlite",
            "zlib",
            "libffi",
            "xtl",
            "pyjs",
            "xeus",
            "xeus-lite" } },
        { "emscripten-obspy",
          { "https://repo.prefix.dev/emscripten-forge-4x", "https://repo.prefix.dev/conda-forge" },
          "emscripten-wasm32",
          { "python", "obspy", "matplotlib", "numpy", "scipy", "pyjs", "requests", "pyodide-http" } },
    };

    for (const auto& c : cases)
    {
        DYNAMIC_SECTION(c.name)
        {
            ctx.channels = c.channels;
            ctx.platform = c.platform;
            ctx.offline = false;
            ctx.use_sharded_repodata = true;

            auto channel_context = ChannelContext::make_conda_compatible(ctx);
            init_channels(ctx, channel_context);

            libsolv::Database libsolv_db{
                channel_context.params(),
                { ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                                     : libsolv::MatchSpecParser::Libsolv },
            };
            MultiPackageCache package_caches{ { cache_dir }, ctx.validation_params };
            auto load = load_channels(
                ctx,
                channel_context,
                libsolv_db,
                package_caches,
                extract_root_packages(c.specs)
            );
            REQUIRE(load.has_value());

            libsolv_db.add_repo_from_packages(
                get_virtual_packages(ctx.platform),
                "virtual",
                solver::libsolv::PipAsPythonDependency::No
            );

            const auto request = make_request(c.specs, ctx.solver_flags);

            auto libsolv_outcome = libsolv::Solver().solve(
                libsolv_db,
                request,
                ctx.experimental_matchspec_parsing ? libsolv::MatchSpecParser::Mamba
                                                   : libsolv::MatchSpecParser::Libsolv
            );
            REQUIRE(libsolv_outcome.has_value());
            REQUIRE(std::holds_alternative<Solution>(libsolv_outcome.value()));
            const auto& libsolv_solution = std::get<Solution>(libsolv_outcome.value());

            auto resolvo_db = make_resolvo_db_from_libsolv(libsolv_db);
            auto resolvo_outcome = mamba::solver::resolvo::Solver().solve(resolvo_db, request);
            REQUIRE(resolvo_outcome.has_value());
            const auto& resolvo_solution = std::get<Solution>(resolvo_outcome.value());

            REQUIRE(as_key_set(resolvo_solution) == as_key_set(libsolv_solution));
        }
    }
}
