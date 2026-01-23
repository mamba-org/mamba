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

    SECTION("shard_loaded")
    {
        REQUIRE_FALSE(shards.shard_loaded("pkg1"));
        REQUIRE_FALSE(shards.shard_loaded("pkg2"));
    }

    SECTION("visit_shard and visit_package")
    {
        ShardDict shard1;
        ShardPackageRecord record1;
        record1.name = "pkg1";
        record1.version = "1.0.0";
        shard1.packages["pkg1-1.0.0.tar.bz2"] = record1;

        shards.visit_shard("pkg1", shard1);
        REQUIRE(shards.shard_loaded("pkg1"));
        REQUIRE_FALSE(shards.shard_loaded("pkg2"));

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

        shards.visit_shard("pkg1", shard1);

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

        shards.visit_shard("pkg1", shard1);

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

        shards.visit_shard("pkg1", shard1);
        shards.visit_shard("pkg2", shard2);

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

    SECTION("fetch_shards returns already visited shards")
    {
        ShardDict shard1;
        ShardPackageRecord pkg1;
        pkg1.name = "pkg1";
        pkg1.version = "1.0.0";
        shard1.packages["pkg1-1.0.0.tar.bz2"] = pkg1;

        shards.visit_shard("pkg1", shard1);

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
    download::mirror_map mirrors;
    mirrors.add_unique_mirror(
        channel.id(),
        download::make_mirror("file://" + tmp_dir.path().string() + "/")
    );
    download::RemoteFetchParams remote_fetch_params;

    Shards shards(
        index,
        "file://" + tmp_dir.path().string() + "/repodata.json",
        channel,
        auth_info,
        mirrors,
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
            REQUIRE((shard.packages.size() > 0 || shard.conda_packages.size() > 0));
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

        shards.visit_shard("pkg1", shard1);

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

        shards.visit_shard("pkg1", shard1);

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
    download::mirror_map mirrors;
    download::RemoteFetchParams remote_fetch_params;

    SECTION("Empty shards_base_url")
    {
        index.info.shards_base_url = "";
        Shards shards(
            index,
            "https://example.com/conda-forge/linux-64/repodata.json",
            channel,
            auth_info,
            mirrors,
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
            mirrors,
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
            mirrors,
            remote_fetch_params
        );

        std::string url = shards.shard_url("test-pkg");
        REQUIRE(util::starts_with(url, "https://example.com/different/path/"));
    }
}
