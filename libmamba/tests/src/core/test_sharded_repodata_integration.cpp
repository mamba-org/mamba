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
#include "mamba/core/shard_traversal.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/solver/libsolv/database.hpp"

#include "mambatests.hpp"

using namespace mamba;

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
