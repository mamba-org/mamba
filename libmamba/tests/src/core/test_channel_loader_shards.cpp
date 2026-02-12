// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/solver/libsolv/database.hpp"

#include "mambatests.hpp"

using namespace mamba;

TEST_CASE("load_channels with root_packages", "[mamba::core][mamba::api::channel_loader]")
{
    auto& ctx = mambatests::context();
    ctx.channels = { "conda-forge" };
    ctx.repodata_use_shards = true;

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    solver::libsolv::Database db{ channel_context.params() };
    MultiPackageCache package_caches(ctx.pkgs_dirs, ctx.validation_params);

    SECTION("Empty root_packages")
    {
        auto result = load_channels(ctx, channel_context, db, package_caches, {});
        REQUIRE(result.has_value());
    }

    SECTION("With root_packages")
    {
        std::vector<std::string> root_packages = { "python" };
        auto result = load_channels(ctx, channel_context, db, package_caches, root_packages);
        REQUIRE(result.has_value());
    }
}
