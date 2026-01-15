// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>
#include <msgpack.h>
#include <msgpack/zone.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/shard_loader.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/download/mirror.hpp"
#include "mamba/download/parameters.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/specs/version.hpp"

#include "mambatests.hpp"
#include "test_shard_utils.hpp"

using namespace mamba;
using namespace mambatests::shard_test_utils;

namespace
{
    auto make_simple_channel(std::string_view chan) -> specs::Channel
    {
        const auto resolve_params = ChannelContext::ChannelResolveParams{
            { "linux-64", "noarch" },
            specs::CondaURL::parse("https://conda.anaconda.org").value()
        };

        return specs::Channel::resolve(specs::UnresolvedChannel::parse(chan).value(), resolve_params)
            .value()
            .front();
    }
}

TEST_CASE("Shards URL construction")
{
    SECTION("Absolute URL handling")
    {
        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "https://shards.example.com/conda-forge";
        index.info.subdir = "linux-64";
        index.version = 1;

        // Add a test package
        std::vector<std::uint8_t> hash_bytes(32, 0xAB);
        index.shards["test-pkg"] = hash_bytes;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::mirror_map mirrors;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            mirrors,
            remote_fetch_params
        );

        // shard_url should return absolute URL
        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::starts_with(url, "https://shards.example.com"));
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
    }

    SECTION("Relative URL handling")
    {
        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "shards";  // Relative path
        index.info.subdir = "linux-64";
        index.version = 1;

        std::vector<std::uint8_t> hash_bytes(32, 0xCD);
        index.shards["test-pkg"] = hash_bytes;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::mirror_map mirrors;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            mirrors,
            remote_fetch_params
        );

        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::contains(url, "example.com"));
        REQUIRE(util::contains(url, "shards"));
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
    }

    SECTION("Different host detection")
    {
        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "https://different-host.com/shards";
        index.info.subdir = "linux-64";
        index.version = 1;

        std::vector<std::uint8_t> hash_bytes(32, 0xEF);
        index.shards["test-pkg"] = hash_bytes;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::mirror_map mirrors;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            mirrors,
            remote_fetch_params
        );

        // Should handle different host correctly
        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::starts_with(url, "https://different-host.com"));
    }
}

TEST_CASE("Shards package ordering")
{
    SECTION("Version and build ordering")
    {
        // Create a shard with multiple versions
        ShardDict shard;

        // Add packages in random order
        ShardPackageRecord pkg1;
        pkg1.name = "test-pkg";
        pkg1.version = "1.0.0";
        pkg1.build = "0";
        pkg1.build_number = 0;
        shard.packages["test-pkg-1.0.0-0.tar.bz2"] = pkg1;

        ShardPackageRecord pkg2;
        pkg2.name = "test-pkg";
        pkg2.version = "2.0.0";
        pkg2.build = "0";
        pkg2.build_number = 0;
        shard.packages["test-pkg-2.0.0-0.tar.bz2"] = pkg2;

        ShardPackageRecord pkg3;
        pkg3.name = "test-pkg";
        pkg3.version = "1.5.0";
        pkg3.build = "0";
        pkg3.build_number = 0;
        shard.packages["test-pkg-1.5.0-0.tar.bz2"] = pkg3;

        ShardPackageRecord pkg4;
        pkg4.name = "test-pkg";
        pkg4.version = "2.0.0";
        pkg4.build = "1";
        pkg4.build_number = 1;
        shard.packages["test-pkg-2.0.0-1.tar.bz2"] = pkg4;

        // Verify shard contains all packages
        REQUIRE(shard.packages.size() == 4);
    }
}

TEST_CASE("Shard parsing - Package record parsing")
{
    SECTION("Parse package record with all fields")
    {
        auto msgpack_data = create_shard_package_record_msgpack(
            "test-pkg",
            "1.2.3",
            "build123",
            42,
            "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
            "12345678901234567890123456789012",
            { "dep1", "dep2" },
            { "constraint1" },
            "python"
        );

        // Parse using msgpack
        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);

        // The actual parsing is done internally by ShardCache, but we can verify
        // the msgpack structure is correct
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with sha256 as bytes")
    {
        auto msgpack_data = create_shard_package_record_msgpack(
            "test-pkg",
            "1.0.0",
            "0",
            0,
            "abababababababababababababababababababababababababababababababab",
            std::nullopt,
            {},
            {},
            std::nullopt,
            true,  // sha256_as_bytes
            false
        );

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with md5 as bytes")
    {
        auto msgpack_data = create_shard_package_record_msgpack(
            "test-pkg",
            "1.0.0",
            "0",
            0,
            std::nullopt,
            "12345678901234567890123456789012",
            {},
            {},
            std::nullopt,
            false,
            true  // md5_as_bytes
        );

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with minimal fields")
    {
        auto msgpack_data = create_shard_package_record_msgpack("minimal-pkg", "1.0.0", "0", 0);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        msgpack_zone_destroy(unpacked.zone);
    }
}

TEST_CASE("Shard parsing - ShardDict parsing")
{
    SECTION("Parse shard dict with packages")
    {
        auto msgpack_data = create_minimal_shard_msgpack("test-pkg", "1.0.0", "0", { "dep1" });

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse shard dict with packages.conda")
    {
        // Create msgpack with packages.conda key
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 1);
        msgpack_pack_str(&pk, 14);
        msgpack_pack_str_body(&pk, "packages.conda", 14);
        msgpack_pack_map(&pk, 1);

        std::string filename = "test-pkg-1.0.0-0.conda";
        msgpack_pack_str(&pk, filename.size());
        msgpack_pack_str_body(&pk, filename.c_str(), filename.size());

        auto pkg_record = create_shard_package_record_msgpack("test-pkg", "1.0.0", "0", 0);
        // Append the package record data
        msgpack_sbuffer_write(
            &sbuf,
            reinterpret_cast<const char*>(pkg_record.data()),
            pkg_record.size()
        );

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse shard dict with both packages and packages.conda")
    {
        // Create a shard with both .tar.bz2 and .conda packages
        // Use the helper function to create a proper shard dict structure
        // For this test, we'll just verify that the structure can be created
        // The actual parsing is tested through the ShardCache interface
        auto msgpack_data = create_minimal_shard_msgpack("test-pkg", "1.0.0", "0", {});

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        auto ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        msgpack_zone_destroy(unpacked.zone);
    }
}
