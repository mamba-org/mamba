// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <sstream>
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

        auto create_shard_index_msgpack(
            const std::string& base_url,
            const std::string& shards_base_url,
            const std::string& subdir,
            std::size_t version,
            const std::map<std::string, std::vector<std::uint8_t>>& shards
        ) -> std::vector<std::uint8_t>
        {
            return create_shard_index_msgpack_with_version(
                base_url,
                shards_base_url,
                subdir,
                version,
                shards
            );
        }

        auto create_shard_index_msgpack_with_version(
            const std::string& base_url,
            const std::string& shards_base_url,
            const std::string& subdir,
            std::size_t version,
            const std::map<std::string, std::vector<std::uint8_t>>& shards
        ) -> std::vector<std::uint8_t>
        {
            msgpack_sbuffer sbuf;
            msgpack_sbuffer_init(&sbuf);
            msgpack_packer pk;
            msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

            // Start map with 3 keys: "info", "version", "shards"
            msgpack_pack_map(&pk, 3);

            // Key: "info"
            msgpack_pack_str(&pk, 4);
            msgpack_pack_str_body(&pk, "info", 4);

            // Value: info map with 3 keys
            msgpack_pack_map(&pk, 3);

            // info.base_url
            msgpack_pack_str(&pk, 8);
            msgpack_pack_str_body(&pk, "base_url", 8);
            msgpack_pack_str(&pk, base_url.size());
            msgpack_pack_str_body(&pk, base_url.c_str(), base_url.size());

            // info.shards_base_url
            msgpack_pack_str(&pk, 15);
            msgpack_pack_str_body(&pk, "shards_base_url", 15);
            msgpack_pack_str(&pk, shards_base_url.size());
            msgpack_pack_str_body(&pk, shards_base_url.c_str(), shards_base_url.size());

            // info.subdir
            msgpack_pack_str(&pk, 6);
            msgpack_pack_str_body(&pk, "subdir", 6);
            msgpack_pack_str(&pk, subdir.size());
            msgpack_pack_str_body(&pk, subdir.c_str(), subdir.size());

            // Key: "version"
            msgpack_pack_str(&pk, 7);
            msgpack_pack_str_body(&pk, "version", 7);
            msgpack_pack_uint64(&pk, version);

            // Key: "shards"
            msgpack_pack_str(&pk, 6);
            msgpack_pack_str_body(&pk, "shards", 6);

            // Value: shards map
            msgpack_pack_map(&pk, shards.size());
            for (const auto& [package_name, hash_bytes] : shards)
            {
                // Package name as key
                msgpack_pack_str(&pk, package_name.size());
                msgpack_pack_str_body(&pk, package_name.c_str(), package_name.size());

                // Hash bytes as value (binary)
                msgpack_pack_bin(&pk, hash_bytes.size());
                msgpack_pack_bin_body(&pk, hash_bytes.data(), hash_bytes.size());
            }

            std::vector<std::uint8_t> result(
                reinterpret_cast<const std::uint8_t*>(sbuf.data),
                reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
            );

            msgpack_sbuffer_destroy(&sbuf);
            return result;
        }

        auto create_shard_index_msgpack_with_repodata_version(
            const std::string& base_url,
            const std::string& shards_base_url,
            const std::string& subdir,
            std::size_t version,
            const std::map<std::string, std::vector<std::uint8_t>>& shards
        ) -> std::vector<std::uint8_t>
        {
            msgpack_sbuffer sbuf;
            msgpack_sbuffer_init(&sbuf);
            msgpack_packer pk;
            msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

            // Start map with 3 keys: "info", "repodata_version", "shards"
            msgpack_pack_map(&pk, 3);

            // Key: "info"
            msgpack_pack_str(&pk, 4);
            msgpack_pack_str_body(&pk, "info", 4);

            // Value: info map with 3 keys
            msgpack_pack_map(&pk, 3);

            // info.base_url
            msgpack_pack_str(&pk, 8);
            msgpack_pack_str_body(&pk, "base_url", 8);
            msgpack_pack_str(&pk, base_url.size());
            msgpack_pack_str_body(&pk, base_url.c_str(), base_url.size());

            // info.shards_base_url
            msgpack_pack_str(&pk, 15);
            msgpack_pack_str_body(&pk, "shards_base_url", 15);
            msgpack_pack_str(&pk, shards_base_url.size());
            msgpack_pack_str_body(&pk, shards_base_url.c_str(), shards_base_url.size());

            // info.subdir
            msgpack_pack_str(&pk, 6);
            msgpack_pack_str_body(&pk, "subdir", 6);
            msgpack_pack_str(&pk, subdir.size());
            msgpack_pack_str_body(&pk, subdir.c_str(), subdir.size());

            // Key: "repodata_version"
            msgpack_pack_str(&pk, 17);
            msgpack_pack_str_body(&pk, "repodata_version", 17);
            msgpack_pack_uint64(&pk, version);

            // Key: "shards"
            msgpack_pack_str(&pk, 6);
            msgpack_pack_str_body(&pk, "shards", 6);

            // Value: shards map
            msgpack_pack_map(&pk, shards.size());
            for (const auto& [package_name, hash_bytes] : shards)
            {
                // Package name as key
                msgpack_pack_str(&pk, package_name.size());
                msgpack_pack_str_body(&pk, package_name.c_str(), package_name.size());

                // Hash bytes as value (binary)
                msgpack_pack_bin(&pk, hash_bytes.size());
                msgpack_pack_bin_body(&pk, hash_bytes.data(), hash_bytes.size());
            }

            std::vector<std::uint8_t> result(
                reinterpret_cast<const std::uint8_t*>(sbuf.data),
                reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
            );

            msgpack_sbuffer_destroy(&sbuf);
            return result;
        }

        auto create_shard_package_record_msgpack(
            const std::string& name,
            const std::string& version,
            const std::string& build,
            std::size_t build_number,
            const std::optional<std::string>& sha256,
            const std::optional<std::string>& md5,
            const std::vector<std::string>& depends,
            const std::vector<std::string>& constrains,
            const std::optional<std::string>& noarch,
            bool sha256_as_bytes,
            bool md5_as_bytes
        ) -> std::vector<std::uint8_t>
        {
            msgpack_sbuffer sbuf;
            msgpack_sbuffer_init(&sbuf);
            msgpack_packer pk;
            msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

            // Count non-optional fields
            std::size_t field_count = 4;  // name, version, build, build_number
            if (sha256.has_value())
            {
                field_count++;
            }
            if (md5.has_value())
            {
                field_count++;
            }
            if (!depends.empty())
            {
                field_count++;
            }
            if (!constrains.empty())
            {
                field_count++;
            }
            if (noarch.has_value())
            {
                field_count++;
            }

            msgpack_pack_map(&pk, field_count);

            // name
            msgpack_pack_str(&pk, 4);
            msgpack_pack_str_body(&pk, "name", 4);
            msgpack_pack_str(&pk, name.size());
            msgpack_pack_str_body(&pk, name.c_str(), name.size());

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

            // build_number
            msgpack_pack_str(&pk, 12);
            msgpack_pack_str_body(&pk, "build_number", 12);
            msgpack_pack_uint64(&pk, build_number);

            // sha256 (optional)
            if (sha256.has_value())
            {
                msgpack_pack_str(&pk, 5);
                msgpack_pack_str_body(&pk, "sha256", 5);
                if (sha256_as_bytes)
                {
                    // Convert hex string to bytes
                    std::vector<std::uint8_t> hash_bytes;
                    hash_bytes.reserve(sha256->size() / 2);
                    for (size_t i = 0; i < sha256->size(); i += 2)
                    {
                        if (i + 1 < sha256->size())
                        {
                            std::string byte_str = sha256->substr(i, 2);
                            hash_bytes.push_back(
                                static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16))
                            );
                        }
                    }
                    msgpack_pack_bin(&pk, hash_bytes.size());
                    msgpack_pack_bin_body(&pk, hash_bytes.data(), hash_bytes.size());
                }
                else
                {
                    msgpack_pack_str(&pk, sha256->size());
                    msgpack_pack_str_body(&pk, sha256->c_str(), sha256->size());
                }
            }

            // md5 (optional)
            if (md5.has_value())
            {
                msgpack_pack_str(&pk, 3);
                msgpack_pack_str_body(&pk, "md5", 3);
                if (md5_as_bytes)
                {
                    // Convert hex string to bytes
                    std::vector<std::uint8_t> hash_bytes;
                    hash_bytes.reserve(md5->size() / 2);
                    for (size_t i = 0; i < md5->size(); i += 2)
                    {
                        if (i + 1 < md5->size())
                        {
                            std::string byte_str = md5->substr(i, 2);
                            hash_bytes.push_back(
                                static_cast<std::uint8_t>(std::stoul(byte_str, nullptr, 16))
                            );
                        }
                    }
                    msgpack_pack_bin(&pk, hash_bytes.size());
                    msgpack_pack_bin_body(&pk, hash_bytes.data(), hash_bytes.size());
                }
                else
                {
                    msgpack_pack_str(&pk, md5->size());
                    msgpack_pack_str_body(&pk, md5->c_str(), md5->size());
                }
            }

            // depends (optional)
            if (!depends.empty())
            {
                msgpack_pack_str(&pk, 7);
                msgpack_pack_str_body(&pk, "depends", 7);
                msgpack_pack_array(&pk, depends.size());
                for (const auto& dep : depends)
                {
                    msgpack_pack_str(&pk, dep.size());
                    msgpack_pack_str_body(&pk, dep.c_str(), dep.size());
                }
            }

            // constrains (optional)
            if (!constrains.empty())
            {
                msgpack_pack_str(&pk, 10);
                msgpack_pack_str_body(&pk, "constrains", 10);
                msgpack_pack_array(&pk, constrains.size());
                for (const auto& constraint : constrains)
                {
                    msgpack_pack_str(&pk, constraint.size());
                    msgpack_pack_str_body(&pk, constraint.c_str(), constraint.size());
                }
            }

            // noarch (optional)
            if (noarch.has_value())
            {
                msgpack_pack_str(&pk, 6);
                msgpack_pack_str_body(&pk, "noarch", 6);
                msgpack_pack_str(&pk, noarch->size());
                msgpack_pack_str_body(&pk, noarch->c_str(), noarch->size());
            }

            std::vector<std::uint8_t> result(
                reinterpret_cast<const std::uint8_t*>(sbuf.data),
                reinterpret_cast<const std::uint8_t*>(sbuf.data + sbuf.size)
            );

            msgpack_sbuffer_destroy(&sbuf);
            return result;
        }
    }
}
