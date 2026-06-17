// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/history.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"

#include "mambatests.hpp"

namespace mamba
{
    std::vector<std::string> extract_package_names_from_specs(const std::vector<std::string>& specs);
    std::vector<std::string>
    extract_exact_package_names_from_specs(const std::vector<std::string>& specs);
    void add_python_related_roots_if_python_requested(std::vector<std::string>& root_packages);
    std::vector<std::string> build_sharded_root_packages(
        const Context& ctx,
        ChannelContext& channel_context,
        const std::vector<std::string>& raw_specs
    );
}

using namespace mamba;

TEST_CASE("repoquery sharded roots extraction", "[mamba::api][repoquery]")
{
    SECTION("extract names from valid MatchSpecs")
    {
        const std::vector<std::string> specs = {
            "xtensor=0.24.5",
            "conda-forge::python>=3.11",
            "xsimd[version='>=13']",
        };

        const auto names = extract_package_names_from_specs(specs);
        REQUIRE(names == std::vector<std::string>{ "xtensor", "python", "xsimd" });
    }

    SECTION("ignore name-free specs and keep valid names")
    {
        const std::vector<std::string> specs = {
            "numpy<2",
            ">=1.0",
        };

        const auto names = extract_package_names_from_specs(specs);
        REQUIRE(names == std::vector<std::string>{ "numpy" });
    }

    SECTION("empty when all specs are name-free")
    {
        const std::vector<std::string> specs = {
            ">=1.0",
            "<2",
        };

        const auto names = extract_package_names_from_specs(specs);
        REQUIRE(names.empty());
    }

    SECTION("python roots include pip and python_abi")
    {
        std::vector<std::string> root_packages = { "python", "numpy" };
        add_python_related_roots_if_python_requested(root_packages);
        REQUIRE(root_packages == std::vector<std::string>{ "python", "numpy", "pip", "python_abi" });
    }

    SECTION("python roots do not duplicate existing pip and python_abi")
    {
        std::vector<std::string> root_packages = { "python", "pip", "python_abi" };
        add_python_related_roots_if_python_requested(root_packages);
        REQUIRE(root_packages == std::vector<std::string>{ "python", "pip", "python_abi" });
    }
}

TEST_CASE("repoquery search exact roots extraction", "[mamba::api][repoquery]")
{
    SECTION("extract exact names")
    {
        const std::vector<std::string> specs = {
            "xtensor",
            "python=3.12",
        };

        const auto names = extract_exact_package_names_from_specs(specs);
        REQUIRE(names == std::vector<std::string>{ "xtensor", "python" });
    }

    SECTION("reject glob queries")
    {
        const std::vector<std::string> specs = {
            "xtensor*",
        };

        const auto names = extract_exact_package_names_from_specs(specs);
        REQUIRE(names.empty());
    }

    SECTION("reject mixed exact and glob queries")
    {
        const std::vector<std::string> specs = {
            "xtensor",
            "xsimd*",
        };

        const auto names = extract_exact_package_names_from_specs(specs);
        REQUIRE(names.empty());
    }

    SECTION("skip invalid MatchSpec and keep exact names")
    {
        const std::vector<std::string> specs = {
            "xtensor",
            "python[version='>=3.11'",
            "xsimd=13",
        };

        const auto names = extract_exact_package_names_from_specs(specs);
        REQUIRE(names == std::vector<std::string>{ "xtensor", "xsimd" });
    }
}

TEST_CASE(
    "build_sharded_root_packages includes history-requested names for empty specs",
    "[mamba::api][repoquery]"
)
{
    auto& ctx = mambatests::context();
    const auto saved_target_prefix = ctx.prefix_params.target_prefix;
    on_scope_exit restore_target_prefix{ [&]
                                         { ctx.prefix_params.target_prefix = saved_target_prefix; } };

    const TemporaryDirectory tmp_dir;
    const fs::u8path conda_meta = tmp_dir.path() / "conda-meta";
    fs::create_directories(conda_meta);
    ctx.prefix_params.target_prefix = tmp_dir.path();

    ChannelContext channel_context = ChannelContext::make_conda_compatible(ctx);
    History history(tmp_dir.path(), channel_context);
    History::UserRequest req;
    req.date = "2026-04-27 00:00:00";
    req.cmd = "update --all";
    req.conda_version = "25.0.0";
    req.update = { "conda-smithy" };
    history.add_entry(req);

    const auto root_packages = build_sharded_root_packages(ctx, channel_context, {});
    REQUIRE(
        std::find(root_packages.begin(), root_packages.end(), "conda-smithy") != root_packages.end()
    );
}
