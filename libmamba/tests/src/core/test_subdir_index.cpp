// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <fstream>
#include <sstream>

#include <catch2/catch_all.hpp>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/string.hpp"

#include "mambatests.hpp"

using namespace mamba;

namespace
{
    [[nodiscard]] auto is_in_directory(const fs::u8path& dir, const fs::u8path& file) -> bool
    {
        auto abs_dir = fs::absolute(dir).lexically_normal();
        auto abs_file = fs::absolute(file).lexically_normal();
        return abs_file.parent_path() == abs_dir;
    }

    [[nodiscard]] auto file_to_string(const fs::u8path& filename) -> std::string
    {
        std::ifstream file(filename.string());
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }
}

TEST_CASE("SubdirIndexLoader", "[mamba::core][mamba::core::SubdirIndexLoader]")
{
    auto channel_context = ChannelContext(
        ChannelContext::ChannelResolveParams{
            { "linux-64", "osx-64", "noarch" },
            specs::CondaURL::parse("https://conda.anaconda.org").value() },
        {}
    );

    const auto qs_channel = channel_context.make_channel("quantstack").front();
    const auto local_repo_path = mambatests::repo_dir / "micromamba/test-server/repo/";
    const auto local_channel = channel_context.make_channel(local_repo_path.string()).front();

    auto mirrors = download::mirror_map();
    for (const auto& chan : { qs_channel, local_channel })
    {
        mirrors.add_unique_mirror(chan.id(), download::make_mirror(chan.url().str()));
    }

    SECTION("Create a subdir loader")
    {
        constexpr auto platform = "mamba-128";
        constexpr auto repodata_filename = "foo.json";

        const auto tmp_dir = TemporaryDirectory();
        auto caches = MultiPackageCache({ tmp_dir.path() }, ValidationParams{});

        auto subdir = SubdirIndexLoader::create(
            {},
            channel_context,
            qs_channel,
            platform,
            caches,
            repodata_filename
        );

        REQUIRE(subdir.has_value());
        CHECK_FALSE(subdir->is_noarch());
        CHECK_FALSE(subdir->is_local());
        CHECK(subdir->channel() == qs_channel);
        CHECK(subdir->name() == "quantstack/mamba-128");
        CHECK(subdir->channel_id() == "quantstack");
        CHECK(subdir->platform() == platform);
        CHECK(
            subdir->repodata_url()
            == specs::CondaURL::parse("https://conda.anaconda.org/quantstack/mamba-128/foo.json").value()
        );
        const auto& metadata = subdir->metadata();
        CHECK(metadata.url() == "");
        CHECK(metadata.etag() == "");

        CHECK_FALSE(subdir->valid_cache_found());
        CHECK_FALSE(subdir->valid_libsolv_cache_path().has_value());
        CHECK_FALSE(subdir->valid_json_cache_path().has_value());
    }

    SECTION("Download indexes")
    {
        const auto tmp_dir = TemporaryDirectory();
        auto caches = MultiPackageCache({ tmp_dir.path() }, ValidationParams{});

        const auto params = SubdirParams{
            /* .local_repodata_ttl */ 1000000,
            /* .offline */ false,
            /* .use_index_cache */ true,
            /* .repodata_use_zst */ true,
        };
        auto subdirs = std::array{
            SubdirIndexLoader::create(params, channel_context, qs_channel, "linux-64", caches).value(),
            SubdirIndexLoader::create(params, channel_context, qs_channel, "noarch", caches).value(),
        };

        auto result = SubdirIndexLoader::download_required_indexes(subdirs, params, {}, mirrors, {}, {});
        REQUIRE(result.has_value());

        const auto cache_dir = tmp_dir.path() / "cache";

        for (const auto& subdir : subdirs)
        {
            CHECK(subdir.valid_cache_found());
            CHECK(is_in_directory(cache_dir, subdir.valid_json_cache_path().value()));
            CHECK(util::contains(file_to_string(subdir.valid_json_cache_path().value()), "packages"));
            CHECK_FALSE(subdir.valid_libsolv_cache_path().has_value());
            CHECK(is_in_directory(cache_dir, subdir.writable_libsolv_cache_path()));
        }
    }

    SECTION("No download offline")
    {
        const auto tmp_dir = TemporaryDirectory();
        auto caches = MultiPackageCache({ tmp_dir.path() }, ValidationParams{});

        const auto params = SubdirParams{
            /* .local_repodata_ttl */ 1000000,
            /* .offline */ true,
            /* .use_index_cache */ true,
            /* .repodata_use_zst */ true,
        };
        auto subdirs = std::array{
            SubdirIndexLoader::create(params, channel_context, qs_channel, "linux-64", caches).value(),
            SubdirIndexLoader::create(params, channel_context, qs_channel, "noarch", caches).value(),
        };

        auto result = SubdirIndexLoader::download_required_indexes(subdirs, params, {}, mirrors, {}, {});
        REQUIRE(result.has_value());

        const auto cache_dir = tmp_dir.path() / "cache";

        for (const auto& subdir : subdirs)
        {
            CHECK_FALSE(subdir.valid_cache_found());
        }
    }

    SECTION("Local noarch-only repo offline")
    {
        const auto tmp_dir = TemporaryDirectory();
        auto caches = MultiPackageCache({ tmp_dir.path() }, ValidationParams{});

        const auto params = SubdirParams{
            /* .local_repodata_ttl */ 1000000,
            /* .offline */ true,
        };
        auto subdirs = std::array{
            SubdirIndexLoader::create(params, channel_context, local_channel, "linux-64", caches).value(),
            SubdirIndexLoader::create(params, channel_context, local_channel, "noarch", caches).value(),
        };

        auto result = SubdirIndexLoader::download_required_indexes(subdirs, params, {}, mirrors, {}, {});
        REQUIRE(result.has_value());

        CHECK_FALSE(subdirs[0].valid_cache_found());
        CHECK(subdirs[1].valid_cache_found());
        CHECK(subdirs[1].valid_json_cache_path().has_value());
    }

    SECTION("Download indexes repodata ttl")
    {
        const auto tmp_dir = TemporaryDirectory();
        auto caches = MultiPackageCache({ tmp_dir.path() }, ValidationParams{});

        const auto params = SubdirParams{
            /* .local_repodata_ttl */ 0,
            /* .offline */ false,
            /* .use_index_cache */ false,
            /* .repodata_use_zst */ true,
        };
        auto subdirs = std::array{
            SubdirIndexLoader::create(params, channel_context, qs_channel, "linux-64", caches).value(),
            SubdirIndexLoader::create(params, channel_context, qs_channel, "noarch", caches).value(),
        };

        CAPTURE(tmp_dir.path());

        auto result = SubdirIndexLoader::download_required_indexes(subdirs, params, {}, mirrors, {}, {});
        REQUIRE(result.has_value());

        const auto cache_dir = tmp_dir.path() / "cache";

        for (const auto& subdir : subdirs)
        {
            CHECK(subdir.valid_cache_found());
        }

        subdirs = std::array{
            SubdirIndexLoader::create(params, channel_context, qs_channel, "linux-64", caches).value(),
            SubdirIndexLoader::create(params, channel_context, qs_channel, "noarch", caches).value(),
        };
        for (const auto& subdir : subdirs)
        {
            CHECK_FALSE(subdir.valid_cache_found());
        }
    }
}
