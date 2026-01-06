// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef TEST_SHARD_UTILS_HPP
#define TEST_SHARD_UTILS_HPP

#include <cstdint>
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
    }
}

#endif
