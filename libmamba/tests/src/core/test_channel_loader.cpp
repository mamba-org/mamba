// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <map>

#include <catch2/catch_all.hpp>

#include "mamba/api/channel_loader.hpp"
#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/util.hpp"
#include "mamba/solver/libsolv/database.hpp"

#include "mambatests.hpp"

using namespace mamba;

TEST_CASE("init_channels", "[mamba::api][channel_loader]")
{
    auto ctx = Context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    SECTION("Empty channels and mirrored_channels does not add channel mirrors")
    {
        ctx.channels = {};
        ctx.mirrored_channels = {};

        init_channels(ctx, channel_context);

        // mirror_map always has a default PassThroughMirror(""); no channel-specific
        // mirrors should be added when channels are empty.
        REQUIRE_FALSE(ctx.mirrors.has_mirrors("conda-forge"));
    }

    SECTION("Single channel registers mirrors")
    {
        ctx.channels = { "conda-forge" };
        ctx.mirrored_channels = {};

        init_channels(ctx, channel_context);

        for (const auto& location : ctx.channels)
        {
            for (const specs::Channel& channel : channel_context.make_channel(location))
            {
                REQUIRE(ctx.mirrors.has_mirrors(channel.id()));
            }
        }
    }

    SECTION("Mirrored channel registers mirrors")
    {
        ctx.channels = {};
        ctx.mirrored_channels = { { "conda-forge", { "https://conda.anaconda.org/conda-forge" } } };

        init_channels(ctx, channel_context);

        for (const auto& [name, urls] : ctx.mirrored_channels)
        {
            for (const specs::Channel& channel : channel_context.make_channel(name, urls))
            {
                REQUIRE(ctx.mirrors.has_mirrors(channel.id()));
            }
        }
    }

    SECTION("Regular channel skipped when in mirrored_channels")
    {
        ctx.channels = { "conda-forge" };
        ctx.mirrored_channels = { { "conda-forge", { "https://conda.anaconda.org/conda-forge" } } };

        init_channels(ctx, channel_context);

        // conda-forge is in mirrored_channels so it was processed there; still should have mirrors
        REQUIRE(ctx.mirrors.has_mirrors("conda-forge"));
    }
}

TEST_CASE("init_channels_from_package_urls", "[mamba::api][channel_loader]")
{
    auto ctx = Context();
    auto channel_context = ChannelContext::make_conda_compatible(ctx);

    SECTION("Package URL spec registers mirrors for package channel")
    {
        // Valid conda package URL; channel will be conda-forge
        std::vector<std::string> specs = {
            "https://conda.anaconda.org/conda-forge/linux-64/python-3.11.0-h1234567_0.conda"
        };

        init_channels_from_package_urls(ctx, channel_context, specs);

        REQUIRE(ctx.mirrors.has_mirrors("conda-forge"));
    }
}

TEST_CASE("load_channels", "[mamba::api][channel_loader]")
{
    // Use test singletons so Console/progress bar are initialized (avoids SIGABRT)
    Context& ctx = mambatests::context();

    // Save and restore context so we don't affect other tests (e.g. test_configuration
    // expects default ssl_verify / config state)
    struct ContextGuard
    {
        Context& ctx;
        std::vector<std::string> channels;
        std::map<std::string, std::vector<std::string>> mirrored_channels;
        std::vector<fs::u8path> pkgs_dirs;
        bool offline;
        std::string ssl_verify;
        std::string channel_alias;
        std::map<std::string, std::string> proxy_servers;

        explicit ContextGuard(Context& c)
            : ctx(c)
            , channels(c.channels)
            , mirrored_channels(c.mirrored_channels)
            , pkgs_dirs(c.pkgs_dirs)
            , offline(c.offline)
            , ssl_verify(c.remote_fetch_params.ssl_verify)
            , channel_alias(c.channel_alias)
            , proxy_servers(c.remote_fetch_params.proxy_servers)
        {
        }

        ~ContextGuard()
        {
            ctx.channels = std::move(channels);
            ctx.mirrored_channels = std::move(mirrored_channels);
            ctx.pkgs_dirs = std::move(pkgs_dirs);
            ctx.offline = offline;
            ctx.remote_fetch_params.ssl_verify = std::move(ssl_verify);
            ctx.channel_alias = std::move(channel_alias);
            ctx.remote_fetch_params.proxy_servers = std::move(proxy_servers);
        }
    };

    ContextGuard guard(ctx);

    ctx.channels = {};
    ctx.mirrored_channels = {};
    ctx.pkgs_dirs = {};
    ctx.offline = true;

    auto channel_context = ChannelContext::make_conda_compatible(ctx);
    auto database = solver::libsolv::Database(channel_context.params());
    const auto tmp_dir = TemporaryDirectory();
    auto package_caches = MultiPackageCache({ tmp_dir.path() }, ValidationParams{});

    auto result = load_channels(ctx, channel_context, database, package_caches);

    REQUIRE(result.has_value());
    REQUIRE(database.repo_count() == 0);
}

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
