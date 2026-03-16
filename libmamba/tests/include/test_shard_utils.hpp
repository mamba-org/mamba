// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef TEST_SHARD_UTILS_HPP
#define TEST_SHARD_UTILS_HPP

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <msgpack.h>
#include <msgpack/zone.h>
#include <zstd.h>

#include "mamba/core/shard_types.hpp"

namespace mambatests
{
    /**
     * Helper functions for creating test shard data.
     */
    namespace shard_test_utils
    {
        /**
         * Create a minimal valid msgpack-serialized ShardDict.
         *
         * @param package_name Name of the package
         * @param version Version string
         * @param build Build string
         * @param depends Optional dependencies
         * @return Serialized msgpack data
         */
        auto create_minimal_shard_msgpack(
            const std::string& package_name,
            const std::string& version = "1.0.0",
            const std::string& build = "0",
            const std::vector<std::string>& depends = {}
        ) -> std::vector<std::uint8_t>;

        /**
         * Compress data with zstd.
         *
         * @param data Data to compress
         * @return Compressed data
         */
        auto compress_zstd(const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t>;

        /**
         * Create a valid shard (msgpack + zstd compressed).
         *
         * @param package_name Name of the package
         * @param version Version string
         * @param build Build string
         * @param depends Optional dependencies
         * @return Compressed shard data ready for caching
         */
        auto create_valid_shard_data(
            const std::string& package_name,
            const std::string& version = "1.0.0",
            const std::string& build = "0",
            const std::vector<std::string>& depends = {}
        ) -> std::vector<std::uint8_t>;

        /**
         * Create corrupted/invalid zstd data.
         *
         * @return Invalid compressed data
         */
        auto create_corrupted_zstd_data() -> std::vector<std::uint8_t>;

        /**
         * Create invalid msgpack data.
         *
         * @return Invalid msgpack data
         */
        auto create_invalid_msgpack_data() -> std::vector<std::uint8_t>;

        /**
         * Create large test data for performance tests.
         *
         * @param size_mb Size in megabytes
         * @return Large data vector
         */
        auto create_large_data(std::size_t size_mb) -> std::vector<std::uint8_t>;

        /**
         * Create a minimal valid shard index msgpack.
         *
         * @param base_url Base URL for packages
         * @param shards_base_url Base URL for shards
         * @param subdir Subdirectory (platform)
         * @param version Version number
         * @param shards Map of package names to hash bytes
         * @return Serialized msgpack data
         */
        auto create_shard_index_msgpack(
            const std::string& base_url,
            const std::string& shards_base_url,
            const std::string& subdir,
            std::size_t version,
            const std::map<std::string, std::vector<std::uint8_t>>& shards
        ) -> std::vector<std::uint8_t>;

        /**
         * Create a shard index msgpack with "version" field name.
         */
        auto create_shard_index_msgpack_with_version(
            const std::string& base_url,
            const std::string& shards_base_url,
            const std::string& subdir,
            std::size_t version,
            const std::map<std::string, std::vector<std::uint8_t>>& shards
        ) -> std::vector<std::uint8_t>;

        /**
         * Create a shard index msgpack with "repodata_version" field name.
         */
        auto create_shard_index_msgpack_with_repodata_version(
            const std::string& base_url,
            const std::string& shards_base_url,
            const std::string& subdir,
            std::size_t version,
            const std::map<std::string, std::vector<std::uint8_t>>& shards
        ) -> std::vector<std::uint8_t>;

        /**
         * Hash format enum for specifying how to encode md5/sha256 in msgpack.
         */
        enum class HashFormat
        {
            String,     ///< As a string
            Bytes,      ///< As binary data (BIN type)
            ArrayBytes  ///< As an array of positive integers (bytes)
        };

        /**
         * Create a shard package record msgpack.
         *
         * @param name Package name
         * @param version Package version
         * @param build Build string
         * @param build_number Build number
         * @param sha256 SHA256 hash (optional, can be string, bytes, or array)
         * @param md5 MD5 hash (optional, can be string, bytes, or array)
         * @param depends Dependencies
         * @param constrains Constraints
         * @param noarch Noarch type
         * @param sha256_format Format for sha256 encoding
         * @param md5_format Format for md5 encoding
         * @return Serialized msgpack data
         */
        auto create_shard_package_record_msgpack(
            const std::string& name,
            const std::string& version,
            const std::string& build,
            std::size_t build_number = 0,
            const std::optional<std::string>& sha256 = std::nullopt,
            const std::optional<std::string>& md5 = std::nullopt,
            const std::vector<std::string>& depends = {},
            const std::vector<std::string>& constrains = {},
            const std::optional<std::string>& noarch = std::nullopt,
            HashFormat sha256_format = HashFormat::String,
            HashFormat md5_format = HashFormat::String
        ) -> std::vector<std::uint8_t>;
    }
}

#endif
