// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cstring>
#include <vector>

#include <msgpack.h>
#include <msgpack/zone.h>
#include <zstd.h>

#include "test_shard_utils.hpp"

namespace mambatests
{
    namespace shard_test_utils
    {
        auto create_minimal_shard_msgpack(
            const std::string& package_name,
            const std::string& version,
            const std::string& build,
            const std::vector<std::string>& depends
        ) -> std::vector<std::uint8_t>
        {
            msgpack_sbuffer sbuf;
            msgpack_sbuffer_init(&sbuf);
            msgpack_packer pk;
            msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

            // Start map with "packages" key
            msgpack_pack_map(&pk, 1);  // One key: "packages"

            // Key: "packages"
            msgpack_pack_str(&pk, 8);
            msgpack_pack_str_body(&pk, "packages", 8);

            // Value: map of packages
            msgpack_pack_map(&pk, 1);  // One package

            // Package filename as key
            std::string filename = package_name + "-" + version + "-" + build + ".tar.bz2";
            msgpack_pack_str(&pk, filename.size());
            msgpack_pack_str_body(&pk, filename.c_str(), filename.size());

            // Package record as value (map)
            msgpack_pack_map(&pk, 4);  // name, version, build, depends

            // name
            msgpack_pack_str(&pk, 4);
            msgpack_pack_str_body(&pk, "name", 4);
            msgpack_pack_str(&pk, package_name.size());
            msgpack_pack_str_body(&pk, package_name.c_str(), package_name.size());

            // version
            msgpack_pack_str(&pk, 7);
            msgpack_pack_str_body(&pk, "version", 7);
            msgpack_pack_str(&pk, version.size());
            msgpack_pack_str_body(&pk, version.c_str(), version.size());

            // build
            msgpack_pack_str(&pk, 5);
            msgpack_pack_str_body(&pk, "build", 5);
            msgpack_pack_str(&pk, build.size());
            msgpack_pack_str_body(&pk, build.c_str(), build.size());

            // depends (array)
            msgpack_pack_str(&pk, 7);
            msgpack_pack_str_body(&pk, "depends", 7);
            msgpack_pack_array(&pk, depends.size());
            for (const auto& dep : depends)
            {
                msgpack_pack_str(&pk, dep.size());
                msgpack_pack_str_body(&pk, dep.c_str(), dep.size());
            }

            std::vector<std::uint8_t> result(
                reinterpret_cast<const std::uint8_t*>(sbuf.data),
                reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
            );

            msgpack_sbuffer_destroy(&sbuf);
            return result;
        }

        auto compress_zstd(const std::vector<std::uint8_t>& data) -> std::vector<std::uint8_t>
        {
            ZSTD_CCtx* cctx = ZSTD_createCCtx();
            if (cctx == nullptr)
            {
                return {};
            }

            std::size_t compressed_bound = ZSTD_compressBound(data.size());
            std::vector<std::uint8_t> compressed(compressed_bound);

            std::size_t compressed_size = ZSTD_compress2(
                cctx,
                compressed.data(),
                compressed.size(),
                data.data(),
                data.size()
            );

            ZSTD_freeCCtx(cctx);

            if (ZSTD_isError(compressed_size))
            {
                return {};
            }

            compressed.resize(compressed_size);
            return compressed;
        }

        auto create_valid_shard_data(
            const std::string& package_name,
            const std::string& version,
            const std::string& build,
            const std::vector<std::string>& depends
        ) -> std::vector<std::uint8_t>
        {
            auto msgpack_data = create_minimal_shard_msgpack(package_name, version, build, depends);
            return compress_zstd(msgpack_data);
        }

        auto create_corrupted_zstd_data() -> std::vector<std::uint8_t>
        {
            // Return data that looks like zstd but is corrupted
            return { 0x28, 0xB5, 0x2F, 0xFD, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF };
        }

        auto create_invalid_msgpack_data() -> std::vector<std::uint8_t>
        {
            // Return data that is not valid msgpack
            return { 0xFF, 0xFF, 0xFF, 0xFF };
        }

        auto create_large_data(std::size_t size_mb) -> std::vector<std::uint8_t>
        {
            std::vector<std::uint8_t> data(size_mb * 1024 * 1024);
            // Fill with pattern
            for (std::size_t i = 0; i < data.size(); ++i)
            {
                data[i] = static_cast<std::uint8_t>(i % 256);
            }
            return data;
        }
    }
}
