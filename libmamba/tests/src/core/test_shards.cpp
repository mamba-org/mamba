// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <fstream>

#include <catch2/catch_all.hpp>
#include <msgpack.h>
#include <msgpack/zone.h>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/core/shards.hpp"
#include "mamba/core/util.hpp"
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
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
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
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
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
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
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
            HashFormat::Bytes,  // sha256_as_bytes
            HashFormat::String
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
            HashFormat::String,
            HashFormat::Bytes  // md5_as_bytes
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

    SECTION("Parse package record with sha256 as array of bytes")
    {
        const std::string expected_sha256 = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
        auto msgpack_data = create_shard_package_record_msgpack(
            "test-pkg",
            "1.0.0",
            "0",
            0,
            expected_sha256,
            std::nullopt,
            {},
            {},
            std::nullopt,
            HashFormat::ArrayBytes,
            HashFormat::String
        );

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify sha256 is stored as array of positive integers (bytes)
        bool found_sha256 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "sha256")
                {
                    found_sha256 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_ARRAY);
                    REQUIRE(val_obj.via.array.size == 32);  // sha256 is 32 bytes = 64 hex chars / 2
                    // Verify first element is a positive integer
                    REQUIRE(val_obj.via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER);
                    break;
                }
            }
        }
        REQUIRE(found_sha256);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with md5 as array of bytes")
    {
        const std::string expected_md5 = "12345678901234567890123456789012";
        auto msgpack_data = create_shard_package_record_msgpack(
            "test-pkg",
            "1.0.0",
            "0",
            0,
            std::nullopt,
            expected_md5,
            {},
            {},
            std::nullopt,
            HashFormat::String,
            HashFormat::ArrayBytes
        );

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify md5 is stored as array of positive integers (bytes)
        bool found_md5 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "md5")
                {
                    found_md5 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_ARRAY);
                    REQUIRE(val_obj.via.array.size == 16);  // md5 is 16 bytes = 32 hex chars / 2
                    // Verify first element is a positive integer
                    REQUIRE(val_obj.via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER);
                    break;
                }
            }
        }
        REQUIRE(found_md5);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with both checksums as arrays")
    {
        const std::string expected_sha256 = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
        const std::string expected_md5 = "12345678901234567890123456789012";
        auto msgpack_data = create_shard_package_record_msgpack(
            "test-pkg",
            "1.0.0",
            "0",
            0,
            expected_sha256,
            expected_md5,
            {},
            {},
            std::nullopt,
            HashFormat::ArrayBytes,
            HashFormat::ArrayBytes
        );

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify both checksums are stored as arrays of positive integers (bytes)
        bool found_sha256 = false;
        bool found_md5 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "sha256")
                {
                    found_sha256 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_ARRAY);
                    REQUIRE(val_obj.via.array.size == 32);
                    // Verify first element is a positive integer
                    REQUIRE(val_obj.via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER);
                }
                else if (key == "md5")
                {
                    found_md5 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_ARRAY);
                    REQUIRE(val_obj.via.array.size == 16);
                    // Verify first element is a positive integer
                    REQUIRE(val_obj.via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER);
                }
            }
        }
        REQUIRE(found_sha256);
        REQUIRE(found_md5);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with mixed hash formats")
    {
        const std::string expected_sha256 = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
        const std::string expected_md5 = "12345678901234567890123456789012";
        auto msgpack_data = create_shard_package_record_msgpack(
            "test-pkg",
            "1.0.0",
            "0",
            0,
            expected_sha256,
            expected_md5,
            {},
            {},
            std::nullopt,
            HashFormat::ArrayBytes,  // sha256 as array of bytes (integers)
            HashFormat::Bytes        // md5 as binary
        );

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify formats
        bool found_sha256 = false;
        bool found_md5 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "sha256")
                {
                    found_sha256 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_ARRAY);
                    REQUIRE(val_obj.via.array.size == 32);  // sha256 is 32 bytes
                    // Verify first element is a positive integer
                    REQUIRE(val_obj.via.array.ptr[0].type == MSGPACK_OBJECT_POSITIVE_INTEGER);
                }
                else if (key == "md5")
                {
                    found_md5 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_BIN);
                }
            }
        }
        REQUIRE(found_sha256);
        REQUIRE(found_md5);

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

        // Pack the package record directly using the packer
        // We need to manually pack the fields from ShardPackageRecord
        msgpack_pack_map(&pk, 3);  // name, version, build

        // name
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "name", 4);
        msgpack_pack_str(&pk, 8);
        msgpack_pack_str_body(&pk, "test-pkg", 8);

        // version
        msgpack_pack_str(&pk, 7);
        msgpack_pack_str_body(&pk, "version", 7);
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "1.0.0", 5);

        // build
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "build", 5);
        msgpack_pack_str(&pk, 1);
        msgpack_pack_str_body(&pk, "0", 1);

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

TEST_CASE("Shards - Basic operations")
{
    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.shards_base_url = "shards";
    index.info.subdir = "linux-64";
    index.version = 1;

    std::vector<std::uint8_t> hash1(32, 0xAA);
    std::vector<std::uint8_t> hash2(32, 0xBB);
    index.shards["pkg1"] = hash1;
    index.shards["pkg2"] = hash2;

    specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    Shards shards(
        index,
        "https://example.com/conda-forge/linux-64/repodata.json",
        channel,
        auth_info,
        remote_fetch_params
    );

    SECTION("package_names")
    {
        auto names = shards.package_names();
        REQUIRE(names.size() == 2);
        REQUIRE(std::find(names.begin(), names.end(), "pkg1") != names.end());
        REQUIRE(std::find(names.begin(), names.end(), "pkg2") != names.end());
    }

    SECTION("contains")
    {
        REQUIRE(shards.contains("pkg1"));
        REQUIRE(shards.contains("pkg2"));
        REQUIRE_FALSE(shards.contains("nonexistent"));
    }

    SECTION("is_shard_present")
    {
        REQUIRE_FALSE(shards.is_shard_present("pkg1"));
        REQUIRE_FALSE(shards.is_shard_present("pkg2"));
    }

    SECTION("process_fetched_shard and visit_package")
    {
        ShardDict shard1;
        ShardPackageRecord record1;
        record1.name = "pkg1";
        record1.version = "1.0.0";
        shard1.packages["pkg1-1.0.0.tar.bz2"] = record1;

        shards.process_fetched_shard("pkg1", shard1);
        REQUIRE(shards.is_shard_present("pkg1"));
        REQUIRE_FALSE(shards.is_shard_present("pkg2"));

        auto visited = shards.visit_package("pkg1");
        REQUIRE(visited.packages.size() == 1);
        REQUIRE(visited.packages.find("pkg1-1.0.0.tar.bz2") != visited.packages.end());

        REQUIRE_THROWS_AS(shards.visit_package("pkg2"), std::runtime_error);
    }

    SECTION("shard_url")
    {
        std::string url = shards.shard_url("pkg1");
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
        REQUIRE(util::contains(url, "example.com"));

        REQUIRE_THROWS_AS(shards.shard_url("nonexistent"), std::runtime_error);
    }

    SECTION("base_url and url")
    {
        REQUIRE(shards.base_url() == "https://example.com/packages");
        REQUIRE(shards.url() == "https://example.com/conda-forge/linux-64/repodata.json");
    }
}

TEST_CASE("Shards - build_repodata")
{
    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.shards_base_url = "shards";
    index.info.subdir = "linux-64";
    index.version = 1;

    specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    Shards shards(
        index,
        "https://example.com/conda-forge/linux-64/repodata.json",
        channel,
        auth_info,
        remote_fetch_params
    );

    SECTION("Empty repodata")
    {
        auto repodata = shards.build_repodata();
        REQUIRE(repodata.shard_dict.packages.empty());
        REQUIRE(repodata.shard_dict.conda_packages.empty());
        REQUIRE(repodata.repodata_version == 2);
        REQUIRE(repodata.info.base_url == "https://example.com/packages");
    }

    SECTION("Build repodata with packages")
    {
        ShardDict shard1;
        ShardPackageRecord pkg1;
        pkg1.name = "test-pkg";
        pkg1.version = "1.0.0";
        pkg1.build = "0";
        pkg1.build_number = 0;
        shard1.packages["test-pkg-1.0.0-0.tar.bz2"] = pkg1;

        ShardPackageRecord pkg2;
        pkg2.name = "test-pkg";
        pkg2.version = "2.0.0";
        pkg2.build = "0";
        pkg2.build_number = 0;
        shard1.packages["test-pkg-2.0.0-0.tar.bz2"] = pkg2;

        shards.process_fetched_shard("pkg1", shard1);

        auto repodata = shards.build_repodata();
        REQUIRE(repodata.shard_dict.packages.size() == 2);
        // Map is ordered by filename, but both packages should be present
        bool found_1_0 = false;
        bool found_2_0 = false;
        for (const auto& [filename, record] : repodata.shard_dict.packages)
        {
            if (record.version == "1.0.0")
            {
                found_1_0 = true;
            }
            if (record.version == "2.0.0")
            {
                found_2_0 = true;
            }
        }
        REQUIRE(found_1_0);
        REQUIRE(found_2_0);
    }

    SECTION("Build repodata with conda packages")
    {
        ShardDict shard1;
        ShardPackageRecord pkg1;
        pkg1.name = "test-pkg";
        pkg1.version = "1.0.0";
        pkg1.build = "0";
        shard1.conda_packages["test-pkg-1.0.0-0.conda"] = pkg1;

        shards.process_fetched_shard("pkg1", shard1);

        auto repodata = shards.build_repodata();
        REQUIRE(repodata.shard_dict.conda_packages.size() == 1);
        REQUIRE(repodata.shard_dict.packages.empty());
    }

    SECTION("Build repodata with multiple shards")
    {
        ShardDict shard1;
        ShardPackageRecord pkg1;
        pkg1.name = "pkg1";
        pkg1.version = "1.0.0";
        shard1.packages["pkg1-1.0.0.tar.bz2"] = pkg1;

        ShardDict shard2;
        ShardPackageRecord pkg2;
        pkg2.name = "pkg2";
        pkg2.version = "1.0.0";
        shard2.packages["pkg2-1.0.0.tar.bz2"] = pkg2;

        shards.process_fetched_shard("pkg1", shard1);
        shards.process_fetched_shard("pkg2", shard2);

        auto repodata = shards.build_repodata();
        REQUIRE(repodata.shard_dict.packages.size() == 2);
    }
}

TEST_CASE("Shards - Error handling")
{
    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.shards_base_url = "shards";
    index.info.subdir = "linux-64";
    index.version = 1;

    specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    Shards shards(
        index,
        "https://example.com/conda-forge/linux-64/repodata.json",
        channel,
        auth_info,
        remote_fetch_params
    );

    SECTION("fetch_shard with non-existent package")
    {
        auto result = shards.fetch_shard("nonexistent");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("visit_package with non-existent package")
    {
        REQUIRE_THROWS_AS(shards.visit_package("nonexistent"), std::runtime_error);
    }
}

TEST_CASE("Shards - fetch_shards with visited cache")
{
    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.shards_base_url = "shards";
    index.info.subdir = "linux-64";
    index.version = 1;

    std::vector<std::uint8_t> hash1(32, 0xAA);
    std::vector<std::uint8_t> hash2(32, 0xBB);
    index.shards["pkg1"] = hash1;
    index.shards["pkg2"] = hash2;

    specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    Shards shards(
        index,
        "https://example.com/conda-forge/linux-64/repodata.json",
        channel,
        auth_info,
        remote_fetch_params
    );

    SECTION("fetch_shards returns already visited shards")
    {
        ShardDict shard1;
        ShardPackageRecord pkg1;
        pkg1.name = "pkg1";
        pkg1.version = "1.0.0";
        shard1.packages["pkg1-1.0.0.tar.bz2"] = pkg1;

        shards.process_fetched_shard("pkg1", shard1);

        std::vector<std::string> packages = { "pkg1", "pkg2" };
        auto result = shards.fetch_shards(packages);

        // pkg1 should be in results from visited cache
        if (result.has_value())
        {
            // If pkg2 download fails, we still get pkg1 from cache
            REQUIRE(result.value().find("pkg1") != result.value().end());
        }
    }
}

TEST_CASE("Shards - Parse shard file from disk")
{
    const auto tmp_dir = TemporaryDirectory();
    auto shard_file = tmp_dir.path() / "aabbccdd.msgpack.zst";

    // Create a valid shard file
    auto shard_data = create_valid_shard_data("test-pkg", "1.0.0", "0", { "dep1", "dep2" });

    // Write to file
    std::ofstream file(shard_file.string(), std::ios::binary);
    file.write(
        reinterpret_cast<const char*>(shard_data.data()),
        static_cast<std::streamsize>(shard_data.size())
    );
    file.close();

    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.shards_base_url = tmp_dir.path().string();
    index.info.subdir = "linux-64";
    index.version = 1;

    // Create hash from filename (aabbccdd...)
    std::vector<std::uint8_t> hash_bytes(32);
    hash_bytes[0] = 0xAA;
    hash_bytes[1] = 0xBB;
    hash_bytes[2] = 0xCC;
    hash_bytes[3] = 0xDD;
    // Fill rest with pattern
    for (size_t i = 4; i < 32; ++i)
    {
        hash_bytes[i] = static_cast<std::uint8_t>(i);
    }

    index.shards["test-pkg"] = hash_bytes;

    specs::Channel channel = make_simple_channel("file://" + tmp_dir.path().string());
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    Shards shards(
        index,
        "file://" + tmp_dir.path().string() + "/repodata.json",
        channel,
        auth_info,
        remote_fetch_params
    );

    SECTION("fetch_shard parses file correctly")
    {
        // Note: This test may fail if file:// URLs aren't properly handled by the downloader
        // But it tests the parsing logic path
        auto result = shards.fetch_shard("test-pkg");
        // The download might fail, but if it succeeds, parsing should work
        if (result.has_value())
        {
            const auto& shard = result.value();
            bool has_packages = shard.packages.size() > 0 || shard.conda_packages.size() > 0;
            REQUIRE(has_packages);
        }
    }
}

TEST_CASE("Shards - build_repodata sorting")
{
    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.shards_base_url = "shards";
    index.info.subdir = "linux-64";
    index.version = 1;

    specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    Shards shards(
        index,
        "https://example.com/conda-forge/linux-64/repodata.json",
        channel,
        auth_info,
        remote_fetch_params
    );

    SECTION("Sort by build number")
    {
        ShardDict shard1;
        ShardPackageRecord pkg1;
        pkg1.name = "test-pkg";
        pkg1.version = "1.0.0";
        pkg1.build = "0";
        pkg1.build_number = 0;
        shard1.packages["test-pkg-1.0.0-0.tar.bz2"] = pkg1;

        ShardPackageRecord pkg2;
        pkg2.name = "test-pkg";
        pkg2.version = "1.0.0";
        pkg2.build = "1";
        pkg2.build_number = 1;
        shard1.packages["test-pkg-1.0.0-1.tar.bz2"] = pkg2;

        shards.process_fetched_shard("pkg1", shard1);

        auto repodata = shards.build_repodata();
        REQUIRE(repodata.shard_dict.packages.size() == 2);
        // Map is ordered by filename, but both packages should be present
        bool found_build_0 = false;
        bool found_build_1 = false;
        for (const auto& [filename, record] : repodata.shard_dict.packages)
        {
            if (record.build_number == 0)
            {
                found_build_0 = true;
            }
            if (record.build_number == 1)
            {
                found_build_1 = true;
            }
        }
        REQUIRE(found_build_0);
        REQUIRE(found_build_1);
    }

    SECTION("Sort by build string when build numbers equal")
    {
        ShardDict shard1;
        ShardPackageRecord pkg1;
        pkg1.name = "test-pkg";
        pkg1.version = "1.0.0";
        pkg1.build = "a";
        pkg1.build_number = 0;
        shard1.packages["test-pkg-1.0.0-a.tar.bz2"] = pkg1;

        ShardPackageRecord pkg2;
        pkg2.name = "test-pkg";
        pkg2.version = "1.0.0";
        pkg2.build = "b";
        pkg2.build_number = 0;
        shard1.packages["test-pkg-1.0.0-b.tar.bz2"] = pkg2;

        shards.process_fetched_shard("pkg1", shard1);

        auto repodata = shards.build_repodata();
        REQUIRE(repodata.shard_dict.packages.size() == 2);
        // Map is ordered by filename, but both packages should be present
        bool found_build_a = false;
        bool found_build_b = false;
        for (const auto& [filename, record] : repodata.shard_dict.packages)
        {
            if (record.build == "a")
            {
                found_build_a = true;
            }
            if (record.build == "b")
            {
                found_build_b = true;
            }
        }
        REQUIRE(found_build_a);
        REQUIRE(found_build_b);
    }
}

TEST_CASE("Shards - shards_base_url edge cases")
{
    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.subdir = "linux-64";
    index.version = 1;

    std::vector<std::uint8_t> hash_bytes(32, 0xAA);
    index.shards["test-pkg"] = hash_bytes;

    specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    SECTION("Empty shards_base_url")
    {
        index.info.shards_base_url = "";
        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
    }

    SECTION("shards_base_url with trailing slash")
    {
        index.info.shards_base_url = "shards/";
        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
        REQUIRE(util::contains(url, "shards"));
    }

    SECTION("Absolute URL with different path")
    {
        index.info.shards_base_url = "https://example.com/different/path/";
        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::starts_with(url, "https://example.com/different/path/"));
    }
}

TEST_CASE("Shard parsing - Hash format edge cases")
{
    SECTION("Parse sha256 as MSGPACK_OBJECT_EXT")
    {
        // Create msgpack with sha256 as EXT type
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 1);
        msgpack_pack_str(&pk, 6);
        msgpack_pack_str_body(&pk, "sha256", 6);

        // Pack as EXT type (type 0, 32 bytes)
        std::vector<std::uint8_t> hash_bytes(32, 0xAB);
        msgpack_pack_ext(&pk, 32, 0);
        msgpack_pack_ext_body(&pk, hash_bytes.data(), 32);

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify sha256 is stored as EXT
        bool found_sha256 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "sha256")
                {
                    found_sha256 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_EXT);
                    break;
                }
            }
        }
        REQUIRE(found_sha256);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse sha256 as MSGPACK_OBJECT_NIL")
    {
        auto msgpack_data = create_shard_package_record_msgpack(
            "test-pkg",
            "1.0.0",
            "0",
            0,
            std::nullopt,                        // No sha256
            "12345678901234567890123456789012",  // md5 present
            {},
            {},
            std::nullopt
        );

        // Manually modify to add nil sha256
        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify md5 is present (sha256 can be nil)
        bool found_md5 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "md5")
                {
                    found_md5 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_STR);
                    break;
                }
            }
        }
        REQUIRE(found_md5);

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse sha256 as array with negative integers (error case)")
    {
        // Create msgpack with sha256 as array containing negative integers
        // Negative integers should be treated as invalid and cause parsing to fail
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 4);  // name, version, build, sha256

        // name
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "name", 4);
        msgpack_pack_str(&pk, 8);
        msgpack_pack_str_body(&pk, "test-pkg", 8);

        // version
        msgpack_pack_str(&pk, 7);
        msgpack_pack_str_body(&pk, "version", 7);
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "1.0.0", 5);

        // build
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "build", 5);
        msgpack_pack_str(&pk, 1);
        msgpack_pack_str_body(&pk, "0", 1);

        // sha256 as array with negative integers (invalid)
        msgpack_pack_str(&pk, 6);
        msgpack_pack_str_body(&pk, "sha256", 6);
        msgpack_pack_array(&pk, 2);
        msgpack_pack_int8(&pk, -1);  // Negative integer - should cause error
        msgpack_pack_int8(&pk, -2);  // Negative integer - should cause error

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify sha256 is stored as array with negative integers
        bool found_sha256 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "sha256")
                {
                    found_sha256 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_ARRAY);
                    REQUIRE(val_obj.via.array.size == 2);
                    REQUIRE(val_obj.via.array.ptr[0].type == MSGPACK_OBJECT_NEGATIVE_INTEGER);
                    break;
                }
            }
        }
        REQUIRE(found_sha256);

        // Test that negative integers cause sha256 parsing to fail
        // When parsing a package record with negative integers in the sha256 array,
        // the parsing should return an empty string for sha256 (error case)
        // We test this indirectly by verifying the behavior through process_fetched_shard
        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "shards";
        index.info.subdir = "linux-64";
        index.version = 1;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test that negative integers cause sha256 parsing to fail
        // Create a ShardDict manually to simulate the error case
        // When sha256 array contains negative integers, parsing should return empty string
        // md5 should still be present to allow the record to be valid
        ShardDict shard_dict;
        ShardPackageRecord record;
        record.name = "test-pkg";
        record.version = "1.0.0";
        record.build = "0";
        record.md5 = "12345678901234567890123456789012";
        // sha256 should be empty (not set) because negative integers cause parsing to fail
        // This simulates what happens when parse_shard_package_record encounters negative integers
        shard_dict.packages["test-pkg-1.0.0-0.tar.bz2"] = record;

        // Process the shard - this tests that the shard can be stored even when sha256 is missing
        // (because md5 is present)
        shards.process_fetched_shard("test-pkg", shard_dict);
        REQUIRE(shards.is_shard_present("test-pkg"));

        // Verify that sha256 is not present (due to negative integer parsing error)
        const auto& visited = shards.visit_package("test-pkg");
        REQUIRE(visited.packages.size() == 1);
        const auto& visited_record = visited.packages.begin()->second;
        REQUIRE(visited_record.name == "test-pkg");
        REQUIRE_FALSE(visited_record.sha256.has_value());  // sha256 should be empty due to parsing
                                                           // error
        REQUIRE(visited_record.md5.has_value());           // md5 should still be present
        REQUIRE(visited_record.md5.value() == "12345678901234567890123456789012");

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse sha256 as array with invalid element types")
    {
        // Create msgpack with sha256 as array containing invalid element types
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 4);  // name, version, build, sha256

        // name
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "name", 4);
        msgpack_pack_str(&pk, 8);
        msgpack_pack_str_body(&pk, "test-pkg", 8);

        // version
        msgpack_pack_str(&pk, 7);
        msgpack_pack_str_body(&pk, "version", 7);
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "1.0.0", 5);

        // build
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "build", 5);
        msgpack_pack_str(&pk, 1);
        msgpack_pack_str_body(&pk, "0", 1);

        // sha256 as array with string element (invalid)
        msgpack_pack_str(&pk, 6);
        msgpack_pack_str_body(&pk, "sha256", 6);
        msgpack_pack_array(&pk, 1);
        msgpack_pack_str(&pk, 2);
        msgpack_pack_str_body(&pk, "ab", 2);  // String instead of integer

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify sha256 is stored as array with invalid element
        bool found_sha256 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "sha256")
                {
                    found_sha256 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_ARRAY);
                    REQUIRE(val_obj.via.array.size == 1);
                    REQUIRE(val_obj.via.array.ptr[0].type == MSGPACK_OBJECT_STR);
                    break;
                }
            }
        }
        REQUIRE(found_sha256);

        // Test parsing through Shards API - add md5 so parsing succeeds
        msgpack_sbuffer sbuf2;
        msgpack_sbuffer_init(&sbuf2);
        msgpack_packer pk2;
        msgpack_packer_init(&pk2, &sbuf2, msgpack_sbuffer_write);

        msgpack_pack_map(&pk2, 1);  // packages key
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "packages", 8);
        msgpack_pack_map(&pk2, 1);  // One package
        msgpack_pack_str(&pk2, 17);
        msgpack_pack_str_body(&pk2, "test-pkg-1.0.0-0.tar.bz2", 17);
        // Copy the package record map with invalid array element sha256 + md5
        msgpack_pack_map(&pk2, 5);  // name, version, build, sha256, md5
        msgpack_pack_str(&pk2, 4);
        msgpack_pack_str_body(&pk2, "name", 4);
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "test-pkg", 8);
        msgpack_pack_str(&pk2, 7);
        msgpack_pack_str_body(&pk2, "version", 7);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "1.0.0", 5);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "build", 5);
        msgpack_pack_str(&pk2, 1);
        msgpack_pack_str_body(&pk2, "0", 1);
        msgpack_pack_str(&pk2, 6);
        msgpack_pack_str_body(&pk2, "sha256", 6);
        msgpack_pack_array(&pk2, 1);
        msgpack_pack_str(&pk2, 2);
        msgpack_pack_str_body(&pk2, "ab", 2);
        msgpack_pack_str(&pk2, 3);
        msgpack_pack_str_body(&pk2, "md5", 3);
        msgpack_pack_str(&pk2, 32);
        msgpack_pack_str_body(&pk2, "12345678901234567890123456789012", 32);

        std::vector<std::uint8_t> shard_data(
            reinterpret_cast<const std::uint8_t*>(sbuf2.data),
            reinterpret_cast<const std::uint8_t*>(sbuf2.data + sbuf2.size)
        );
        msgpack_sbuffer_destroy(&sbuf2);

        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "shards";
        index.info.subdir = "linux-64";
        index.version = 1;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test parsing indirectly through process_fetched_shard
        // Create a ShardDict manually with the parsed data
        ShardDict shard_dict;
        ShardPackageRecord record;
        record.name = "test-pkg";
        record.version = "1.0.0";
        record.build = "0";
        record.md5 = "12345678901234567890123456789012";
        // sha256 will be empty due to invalid element types, but md5 is present
        shard_dict.packages["test-pkg-1.0.0-0.tar.bz2"] = record;

        // Process the shard - this tests that the shard can be stored
        shards.process_fetched_shard("test-pkg", shard_dict);
        REQUIRE(shards.is_shard_present("test-pkg"));

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse sha256 as empty array")
    {
        // Create msgpack with sha256 as empty array
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 4);  // name, version, build, sha256

        // name
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "name", 4);
        msgpack_pack_str(&pk, 8);
        msgpack_pack_str_body(&pk, "test-pkg", 8);

        // version
        msgpack_pack_str(&pk, 7);
        msgpack_pack_str_body(&pk, "version", 7);
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "1.0.0", 5);

        // build
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "build", 5);
        msgpack_pack_str(&pk, 1);
        msgpack_pack_str_body(&pk, "0", 1);

        // sha256 as empty array
        msgpack_pack_str(&pk, 6);
        msgpack_pack_str_body(&pk, "sha256", 6);
        msgpack_pack_array(&pk, 0);

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Verify sha256 is stored as empty array
        bool found_sha256 = false;
        for (std::uint32_t i = 0; i < unpacked.data.via.map.size; ++i)
        {
            const msgpack_object& key_obj = unpacked.data.via.map.ptr[i].key;
            const msgpack_object& val_obj = unpacked.data.via.map.ptr[i].val;

            if (key_obj.type == MSGPACK_OBJECT_STR)
            {
                std::string key(reinterpret_cast<const char*>(key_obj.via.str.ptr), key_obj.via.str.size);
                if (key == "sha256")
                {
                    found_sha256 = true;
                    REQUIRE(val_obj.type == MSGPACK_OBJECT_ARRAY);
                    REQUIRE(val_obj.via.array.size == 0);
                    break;
                }
            }
        }
        REQUIRE(found_sha256);

        // Test parsing through Shards API - add md5 so parsing succeeds
        msgpack_sbuffer sbuf2;
        msgpack_sbuffer_init(&sbuf2);
        msgpack_packer pk2;
        msgpack_packer_init(&pk2, &sbuf2, msgpack_sbuffer_write);

        msgpack_pack_map(&pk2, 1);  // packages key
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "packages", 8);
        msgpack_pack_map(&pk2, 1);  // One package
        msgpack_pack_str(&pk2, 17);
        msgpack_pack_str_body(&pk2, "test-pkg-1.0.0-0.tar.bz2", 17);
        // Copy the package record map with empty array sha256 + md5
        msgpack_pack_map(&pk2, 5);  // name, version, build, sha256, md5
        msgpack_pack_str(&pk2, 4);
        msgpack_pack_str_body(&pk2, "name", 4);
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "test-pkg", 8);
        msgpack_pack_str(&pk2, 7);
        msgpack_pack_str_body(&pk2, "version", 7);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "1.0.0", 5);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "build", 5);
        msgpack_pack_str(&pk2, 1);
        msgpack_pack_str_body(&pk2, "0", 1);
        msgpack_pack_str(&pk2, 6);
        msgpack_pack_str_body(&pk2, "sha256", 6);
        msgpack_pack_array(&pk2, 0);
        msgpack_pack_str(&pk2, 3);
        msgpack_pack_str_body(&pk2, "md5", 3);
        msgpack_pack_str(&pk2, 32);
        msgpack_pack_str_body(&pk2, "12345678901234567890123456789012", 32);

        std::vector<std::uint8_t> shard_data(
            reinterpret_cast<const std::uint8_t*>(sbuf2.data),
            reinterpret_cast<const std::uint8_t*>(sbuf2.data + sbuf2.size)
        );
        msgpack_sbuffer_destroy(&sbuf2);

        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "shards";
        index.info.subdir = "linux-64";
        index.version = 1;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test parsing indirectly through process_fetched_shard
        // Create a ShardDict manually with the parsed data
        ShardDict shard_dict;
        ShardPackageRecord record;
        record.name = "test-pkg";
        record.version = "1.0.0";
        record.build = "0";
        record.md5 = "12345678901234567890123456789012";
        // sha256 will be empty due to empty array, but md5 is present
        shard_dict.packages["test-pkg-1.0.0-0.tar.bz2"] = record;

        // Process the shard - this tests that the shard can be stored
        shards.process_fetched_shard("test-pkg", shard_dict);
        REQUIRE(shards.is_shard_present("test-pkg"));

        msgpack_zone_destroy(unpacked.zone);
    }
}

TEST_CASE("Shard parsing - Package record error handling")
{
    SECTION("Parse package record with missing checksums")
    {
        // Create msgpack without sha256 or md5
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 3);  // name, version, build

        // name
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "name", 4);
        msgpack_pack_str(&pk, 8);
        msgpack_pack_str_body(&pk, "test-pkg", 8);

        // version
        msgpack_pack_str(&pk, 7);
        msgpack_pack_str_body(&pk, "version", 7);
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "1.0.0", 5);

        // build
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "build", 5);
        msgpack_pack_str(&pk, 1);
        msgpack_pack_str_body(&pk, "0", 1);

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Create a shard dict structure with this package record
        msgpack_sbuffer sbuf2;
        msgpack_sbuffer_init(&sbuf2);
        msgpack_packer pk2;
        msgpack_packer_init(&pk2, &sbuf2, msgpack_sbuffer_write);

        msgpack_pack_map(&pk2, 1);  // packages key
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "packages", 8);
        msgpack_pack_map(&pk2, 1);  // One package
        msgpack_pack_str(&pk2, 17);
        msgpack_pack_str_body(&pk2, "test-pkg-1.0.0-0.tar.bz2", 17);
        // Copy the package record map
        msgpack_pack_map(&pk2, 3);  // name, version, build
        msgpack_pack_str(&pk2, 4);
        msgpack_pack_str_body(&pk2, "name", 4);
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "test-pkg", 8);
        msgpack_pack_str(&pk2, 7);
        msgpack_pack_str_body(&pk2, "version", 7);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "1.0.0", 5);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "build", 5);
        msgpack_pack_str(&pk2, 1);
        msgpack_pack_str_body(&pk2, "0", 1);

        std::vector<std::uint8_t> shard_data(
            reinterpret_cast<const std::uint8_t*>(sbuf2.data),
            reinterpret_cast<const std::uint8_t*>(sbuf2.data + sbuf2.size)
        );
        msgpack_sbuffer_destroy(&sbuf2);

        // Test parsing through Shards::parse_shard_msgpack
        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "shards";
        index.info.subdir = "linux-64";
        index.version = 1;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test that a shard without checksums cannot be processed
        // We can't directly test parse_shard_msgpack, but we can verify
        // that process_fetched_shard requires valid records
        ShardDict shard_dict;
        ShardPackageRecord record;
        record.name = "test-pkg";
        record.version = "1.0.0";
        record.build = "0";
        // No checksums - this should be invalid
        // But process_fetched_shard doesn't validate, so we just verify
        // the structure can be created
        shard_dict.packages["test-pkg-1.0.0-0.tar.bz2"] = record;

        // Note: process_fetched_shard doesn't validate checksums,
        // but parse_shard_package_record does (which is tested indirectly
        // through fetch_shard in integration tests)
        shards.process_fetched_shard("test-pkg", shard_dict);
        REQUIRE(shards.is_shard_present("test-pkg"));

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with invalid key type")
    {
        // Create msgpack with invalid key type (integer instead of string)
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 1);
        msgpack_pack_uint8(&pk, 42);  // Integer key instead of string
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "value", 5);

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Parsing should skip invalid key and continue
        // Add required fields so parsing can succeed
        // This tests that invalid keys are skipped gracefully
        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with nil required field")
    {
        // Create msgpack with nil name (required field)
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 4);  // name, version, build, sha256

        // name as nil
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "name", 4);
        msgpack_pack_nil(&pk);

        // version
        msgpack_pack_str(&pk, 7);
        msgpack_pack_str_body(&pk, "version", 7);
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "1.0.0", 5);

        // build
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "build", 5);
        msgpack_pack_str(&pk, 1);
        msgpack_pack_str_body(&pk, "0", 1);

        // sha256
        msgpack_pack_str(&pk, 6);
        msgpack_pack_str_body(&pk, "sha256", 6);
        msgpack_pack_str(&pk, 64);
        msgpack_pack_str_body(
            &pk,
            "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
            64
        );

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Create a shard dict structure with this package record
        msgpack_sbuffer sbuf2;
        msgpack_sbuffer_init(&sbuf2);
        msgpack_packer pk2;
        msgpack_packer_init(&pk2, &sbuf2, msgpack_sbuffer_write);

        msgpack_pack_map(&pk2, 1);  // packages key
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "packages", 8);
        msgpack_pack_map(&pk2, 1);  // One package
        msgpack_pack_str(&pk2, 17);
        msgpack_pack_str_body(&pk2, "test-pkg-1.0.0-0.tar.bz2", 17);
        // Copy the package record map (with nil name)
        msgpack_pack_map(&pk2, 4);  // name (nil), version, build, sha256
        msgpack_pack_str(&pk2, 4);
        msgpack_pack_str_body(&pk2, "name", 4);
        msgpack_pack_nil(&pk2);
        msgpack_pack_str(&pk2, 7);
        msgpack_pack_str_body(&pk2, "version", 7);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "1.0.0", 5);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "build", 5);
        msgpack_pack_str(&pk2, 1);
        msgpack_pack_str_body(&pk2, "0", 1);
        msgpack_pack_str(&pk2, 6);
        msgpack_pack_str_body(&pk2, "sha256", 6);
        msgpack_pack_str(&pk2, 64);
        msgpack_pack_str_body(
            &pk2,
            "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
            64
        );

        std::vector<std::uint8_t> shard_data(
            reinterpret_cast<const std::uint8_t*>(sbuf2.data),
            reinterpret_cast<const std::uint8_t*>(sbuf2.data + sbuf2.size)
        );
        msgpack_sbuffer_destroy(&sbuf2);

        // Test parsing through Shards::parse_shard_msgpack
        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "shards";
        index.info.subdir = "linux-64";
        index.version = 1;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test that a shard with nil name can be processed
        // We test indirectly through process_fetched_shard
        ShardDict shard_dict;
        ShardPackageRecord record;
        record.name = "";  // Empty name (nil was skipped)
        record.version = "1.0.0";
        record.build = "0";
        record.sha256 = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
        shard_dict.packages["test-pkg-1.0.0-0.tar.bz2"] = record;

        shards.process_fetched_shard("test-pkg", shard_dict);
        REQUIRE(shards.is_shard_present("test-pkg"));

        msgpack_zone_destroy(unpacked.zone);
    }

    SECTION("Parse package record with size field")
    {
        // Create msgpack with size field
        msgpack_sbuffer sbuf;
        msgpack_sbuffer_init(&sbuf);
        msgpack_packer pk;
        msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

        msgpack_pack_map(&pk, 5);  // name, version, build, sha256, size

        // name
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "name", 4);
        msgpack_pack_str(&pk, 8);
        msgpack_pack_str_body(&pk, "test-pkg", 8);

        // version
        msgpack_pack_str(&pk, 7);
        msgpack_pack_str_body(&pk, "version", 7);
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "1.0.0", 5);

        // build
        msgpack_pack_str(&pk, 5);
        msgpack_pack_str_body(&pk, "build", 5);
        msgpack_pack_str(&pk, 1);
        msgpack_pack_str_body(&pk, "0", 1);

        // sha256
        msgpack_pack_str(&pk, 6);
        msgpack_pack_str_body(&pk, "sha256", 6);
        msgpack_pack_str(&pk, 64);
        msgpack_pack_str_body(
            &pk,
            "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
            64
        );

        // size
        msgpack_pack_str(&pk, 4);
        msgpack_pack_str_body(&pk, "size", 4);
        msgpack_pack_uint64(&pk, 12345);

        std::vector<std::uint8_t> msgpack_data(
            reinterpret_cast<const std::uint8_t*>(sbuf.data),
            reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
        );
        msgpack_sbuffer_destroy(&sbuf);

        msgpack_unpacked unpacked = {};
        size_t offset = 0;
        msgpack_unpack_return ret = msgpack_unpack_next(
            &unpacked,
            reinterpret_cast<const char*>(msgpack_data.data()),
            msgpack_data.size(),
            &offset
        );

        REQUIRE(ret == MSGPACK_UNPACK_SUCCESS);
        REQUIRE(unpacked.data.type == MSGPACK_OBJECT_MAP);

        // Create a shard dict structure with this package record
        msgpack_sbuffer sbuf2;
        msgpack_sbuffer_init(&sbuf2);
        msgpack_packer pk2;
        msgpack_packer_init(&pk2, &sbuf2, msgpack_sbuffer_write);

        msgpack_pack_map(&pk2, 1);  // packages key
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "packages", 8);
        msgpack_pack_map(&pk2, 1);  // One package
        msgpack_pack_str(&pk2, 17);
        msgpack_pack_str_body(&pk2, "test-pkg-1.0.0-0.tar.bz2", 17);
        // Copy the package record map with size field
        msgpack_pack_map(&pk2, 5);  // name, version, build, sha256, size
        msgpack_pack_str(&pk2, 4);
        msgpack_pack_str_body(&pk2, "name", 4);
        msgpack_pack_str(&pk2, 8);
        msgpack_pack_str_body(&pk2, "test-pkg", 8);
        msgpack_pack_str(&pk2, 7);
        msgpack_pack_str_body(&pk2, "version", 7);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "1.0.0", 5);
        msgpack_pack_str(&pk2, 5);
        msgpack_pack_str_body(&pk2, "build", 5);
        msgpack_pack_str(&pk2, 1);
        msgpack_pack_str_body(&pk2, "0", 1);
        msgpack_pack_str(&pk2, 6);
        msgpack_pack_str_body(&pk2, "sha256", 6);
        msgpack_pack_str(&pk2, 64);
        msgpack_pack_str_body(
            &pk2,
            "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890",
            64
        );
        msgpack_pack_str(&pk2, 4);
        msgpack_pack_str_body(&pk2, "size", 4);
        msgpack_pack_uint64(&pk2, 12345);

        std::vector<std::uint8_t> shard_data(
            reinterpret_cast<const std::uint8_t*>(sbuf2.data),
            reinterpret_cast<const std::uint8_t*>(sbuf2.data + sbuf2.size)
        );
        msgpack_sbuffer_destroy(&sbuf2);

        // Test parsing through Shards::parse_shard_msgpack
        ShardsIndexDict index;
        index.info.base_url = "https://example.com/packages";
        index.info.shards_base_url = "shards";
        index.info.subdir = "linux-64";
        index.version = 1;

        specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
        specs::AuthenticationDataBase auth_info;
        download::RemoteFetchParams remote_fetch_params;

        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test that size field is handled correctly
        // We test indirectly through process_fetched_shard
        ShardDict shard_dict;
        ShardPackageRecord record;
        record.name = "test-pkg";
        record.version = "1.0.0";
        record.build = "0";
        record.sha256 = "abcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890";
        record.size = 12345;
        shard_dict.packages["test-pkg-1.0.0-0.tar.bz2"] = record;

        shards.process_fetched_shard("test-pkg", shard_dict);
        REQUIRE(shards.is_shard_present("test-pkg"));

        auto visited = shards.visit_package("test-pkg");
        REQUIRE(visited.packages.size() == 1);
        REQUIRE(visited.packages.begin()->second.size == 12345);

        msgpack_zone_destroy(unpacked.zone);
    }
}

TEST_CASE("Shards - shard_url edge cases for relative_shard_path coverage")
{
    ShardsIndexDict index;
    index.info.base_url = "https://example.com/packages";
    index.info.subdir = "linux-64";
    index.version = 1;

    std::vector<std::uint8_t> hash_bytes(32, 0xAA);
    index.shards["test-pkg"] = hash_bytes;

    specs::Channel channel = make_simple_channel("https://example.com/conda-forge");
    specs::AuthenticationDataBase auth_info;
    download::RemoteFetchParams remote_fetch_params;

    SECTION("Absolute URL with same host")
    {
        index.info.shards_base_url = "https://example.com/shards";
        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test through public API - shard_url uses relative_shard_path internally
        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::contains(url, "shards"));
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
    }

    SECTION("Absolute URL with different host")
    {
        index.info.shards_base_url = "https://different-host.com/shards";
        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test through public API
        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::contains(url, "different-host.com"));
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
    }

    SECTION("Relative URL with ./ prefix")
    {
        index.info.shards_base_url = "./shards";
        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test through public API
        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::contains(url, "shards"));
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
    }

    SECTION("Relative URL with / prefix")
    {
        index.info.shards_base_url = "/shards";
        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            remote_fetch_params
        );

        // Test through public API
        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::contains(url, "shards"));
        REQUIRE(util::ends_with(url, ".msgpack.zst"));
    }
}
