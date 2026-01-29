// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include <fmt/format.h>
#include <msgpack.h>
#include <msgpack/zone.h>
#include <zstd.h>

#include "mamba/core/logging.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/core/shards.hpp"
#include "mamba/core/util.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/version.hpp"
#include "mamba/util/cryptography.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{
    namespace
    {
        // Helper functions to extract values from msgpack_object (C API)
        auto msgpack_object_to_string(const msgpack_object& obj) -> std::string
        {
            if (obj.type == MSGPACK_OBJECT_STR)
            {
                return std::string(obj.via.str.ptr, obj.via.str.size);
            }
            else if (obj.type == MSGPACK_OBJECT_BIN)
            {
                return std::string(obj.via.bin.ptr, obj.via.bin.size);
            }
            throw std::runtime_error("Expected STR or BIN type for string conversion");
        }

        auto msgpack_object_to_uint64(const msgpack_object& obj) -> std::uint64_t
        {
            if (obj.type == MSGPACK_OBJECT_POSITIVE_INTEGER)
            {
                return obj.via.u64;
            }
            else if (obj.type == MSGPACK_OBJECT_NEGATIVE_INTEGER)
            {
                // For negative integers, take absolute value before converting to uint64_t
                // This handles cases where build_number or size might be incorrectly serialized
                // as negative values, which are semantically invalid
                return static_cast<std::uint64_t>(std::abs(obj.via.i64));
            }
            throw std::runtime_error("Expected INTEGER type");
        }

        auto msgpack_object_to_string_array(const msgpack_object& obj) -> std::vector<std::string>
        {
            std::vector<std::string> result;
            if (obj.type == MSGPACK_OBJECT_ARRAY)
            {
                result.reserve(obj.via.array.size);
                for (std::uint32_t i = 0; i < obj.via.array.size; ++i)
                {
                    try
                    {
                        result.push_back(msgpack_object_to_string(obj.via.array.ptr[i]));
                    }
                    catch (const std::exception& e)
                    {
                        LOG_DEBUG << "Failed to parse array element " << i << ": " << e.what();
                        // Skip invalid elements
                    }
                }
            }
            return result;
        }

        /**
         * Extract a hash value (sha256 or md5) from a msgpack object.
         * Handles string, binary, and extension types, converting bytes to hex strings.
         */
        auto msgpack_object_to_hash_string(const msgpack_object& obj) -> std::string
        {
            if (obj.type == MSGPACK_OBJECT_STR)
            {
                return std::string(obj.via.str.ptr, obj.via.str.size);
            }
            else if (obj.type == MSGPACK_OBJECT_BIN)
            {
                // Convert bytes to hex string
                return util::bytes_to_hex_str(
                    reinterpret_cast<const std::byte*>(obj.via.bin.ptr),
                    reinterpret_cast<const std::byte*>(obj.via.bin.ptr + obj.via.bin.size)
                );
            }
            else if (obj.type == MSGPACK_OBJECT_EXT)
            {
                // Handle EXT type (sometimes used for bytes)
                return util::bytes_to_hex_str(
                    reinterpret_cast<const std::byte*>(obj.via.ext.ptr),
                    reinterpret_cast<const std::byte*>(obj.via.ext.ptr + obj.via.ext.size)
                );
            }
            else
            {
                // Try to convert as string
                try
                {
                    return msgpack_object_to_string(obj);
                }
                catch (...)
                {
                    // Return empty string if conversion fails
                    return std::string();
                }
            }
        }

        /**
         * Manually parse a ShardPackageRecord from msgpack object.
         *
         * This handles the case where sha256 and md5 can be either strings or bytes
         * (as per Python TypedDict: NotRequired[str | bytes]).
         */
        auto parse_shard_package_record(const msgpack_object& obj) -> ShardPackageRecord
        {
            ShardPackageRecord record;

            if (obj.type != MSGPACK_OBJECT_MAP)
            {
                throw std::runtime_error("Expected MAP type for ShardPackageRecord");
            }

            for (std::uint32_t i = 0; i < obj.via.map.size; ++i)
            {
                const msgpack_object& key_obj = obj.via.map.ptr[i].key;
                const msgpack_object& val_obj = obj.via.map.ptr[i].val;

                std::string key;
                try
                {
                    key = msgpack_object_to_string(key_obj);
                }
                catch (const std::exception&)
                {
                    continue;
                }

                // Skip nil values for optional fields
                if (val_obj.type == MSGPACK_OBJECT_NIL)
                {
                    // Only skip for optional fields, required fields should not be nil
                    if (key != "sha256" && key != "md5" && key != "constrains" && key != "noarch")
                    {
                        LOG_DEBUG << "Field '" << key << "' is nil in ShardPackageRecord, skipping";
                    }
                    continue;
                }

                try
                {
                    if (key == "name")
                    {
                        record.name = msgpack_object_to_string(val_obj);
                    }
                    else if (key == "version")
                    {
                        record.version = msgpack_object_to_string(val_obj);
                    }
                    else if (key == "build")
                    {
                        record.build = msgpack_object_to_string(val_obj);
                    }
                    else if (key == "build_number")
                    {
                        // Handle both int and uint64
                        record.build_number = msgpack_object_to_uint64(val_obj);
                    }
                    else if (key == "sha256")
                    {
                        record.sha256 = msgpack_object_to_hash_string(val_obj);
                    }
                    else if (key == "md5")
                    {
                        record.md5 = msgpack_object_to_hash_string(val_obj);
                    }
                    else if (key == "depends")
                    {
                        // Handle nil or missing depends
                        if (val_obj.type == MSGPACK_OBJECT_NIL)
                        {
                            record.depends = std::vector<std::string>();
                        }
                        else if (val_obj.type == MSGPACK_OBJECT_ARRAY)
                        {
                            record.depends = msgpack_object_to_string_array(val_obj);
                            if (!record.depends.empty())
                            {
                                LOG_DEBUG << "Parsed dependencies for package '" << record.name
                                          << "': [" << util::join(", ", record.depends) << "]";
                            }
                        }
                        else
                        {
                            LOG_DEBUG << "Field 'depends' has unexpected type: "
                                      << static_cast<int>(val_obj.type);
                        }
                    }
                    else if (key == "constrains")
                    {
                        // Handle nil or missing constrains
                        if (val_obj.type == MSGPACK_OBJECT_NIL)
                        {
                            record.constrains = std::vector<std::string>();
                        }
                        else if (val_obj.type == MSGPACK_OBJECT_ARRAY)
                        {
                            record.constrains = msgpack_object_to_string_array(val_obj);
                        }
                        else
                        {
                            LOG_DEBUG << "Field 'constrains' has unexpected type: "
                                      << static_cast<int>(val_obj.type);
                        }
                    }
                    else if (key == "noarch")
                    {
                        if (val_obj.type == MSGPACK_OBJECT_NIL)
                        {
                            record.noarch = std::nullopt;
                        }
                        else
                        {
                            record.noarch = msgpack_object_to_string(val_obj);
                        }
                    }
                    else if (key == "size")
                    {
                        // Handle size as integer (uint64 or int)
                        record.size = msgpack_object_to_uint64(val_obj);
                    }
                    // Ignore unknown fields (they might be present in the data but not needed)
                }
                catch (const std::exception& e)
                {
                    LOG_WARNING << "Failed to parse field '" << key
                                << "' (type=" << static_cast<int>(val_obj.type)
                                << ") in ShardPackageRecord: " << e.what();
                    // Continue parsing other fields
                }
            }

            return record;
        }
    }

    /******************
     * Shards         *
     ******************/

    Shards::Shards(
        ShardsIndexDict shards_index,
        std::string url,
        specs::Channel channel,
        specs::AuthenticationDataBase auth_info,
        download::RemoteFetchParams remote_fetch_params,
        std::size_t download_threads
    )
        : m_shards_index(std::move(shards_index))
        , m_url(std::move(url))
        , m_channel(std::move(channel))
        , m_auth_info(std::move(auth_info))
        , m_remote_fetch_params(std::move(remote_fetch_params))
        , m_download_threads(download_threads)
    {
    }

    auto Shards::package_names() const -> std::vector<std::string>
    {
        std::vector<std::string> names;
        names.reserve(m_shards_index.shards.size());
        for (const auto& [name, _] : m_shards_index.shards)
        {
            names.push_back(name);
        }
        return names;
    }

    auto Shards::contains(const std::string& package) const -> bool
    {
        return m_shards_index.shards.find(package) != m_shards_index.shards.end();
    }

    auto Shards::shards_base_url() const -> std::string
    {
        if (m_shards_base_url.has_value())
        {
            return *m_shards_base_url;
        }

        std::string shards_base_url_str = m_shards_index.info.shards_base_url;

        // Check if shards_base_url is absolute (has a URL scheme)
        bool is_absolute = util::url_has_scheme(shards_base_url_str);

        std::string result;
        if (is_absolute)
        {
            // For absolute URLs, use as-is
            result = shards_base_url_str;
        }
        else
        {
            // For relative URLs, join with repodata URL
            // url_concat handles slashes automatically, no need for "/" separator
            result = util::url_concat(m_url, shards_base_url_str);
        }

        // Ensure trailing slash
        if (!util::ends_with(result, "/"))
        {
            result += "/";
        }

        m_shards_base_url = result;
        return result;
    }

    auto Shards::shard_url(const std::string& package) const -> std::string
    {
        LOG_DEBUG << "Constructing shard URL for package '" << package
                  << "' from base_url: " << shards_base_url();
        auto it = m_shards_index.shards.find(package);
        if (it == m_shards_index.shards.end())
        {
            LOG_DEBUG << "Package '" << package << "' not found in shard index (index contains "
                      << m_shards_index.shards.size() << " packages)";
            throw std::runtime_error("Package " + package + " not found in shard index");
        }

        // Convert hash bytes to hex string
        std::string hex_hash = util::bytes_to_hex_str(
            reinterpret_cast<const std::byte*>(it->second.data()),
            reinterpret_cast<const std::byte*>(it->second.data() + it->second.size())
        );
        LOG_DEBUG << "Package '" << package << "' found in shard index with hash: " << hex_hash;
        std::string shard_name = hex_hash + ".msgpack.zst";
        std::string url = shards_base_url() + shard_name;
        LOG_DEBUG << "Constructed shard URL for package '" << package << "': " << url;
        return url;
    }

    /**
     * Get the relative path for a shard (for use with download::Request).
     * Returns path relative to channel base, e.g., "linux-64/shards/<hash>.msgpack.zst"
     */
    auto Shards::relative_shard_path(const std::string& package) const -> std::string
    {
        auto it = m_shards_index.shards.find(package);
        if (it == m_shards_index.shards.end())
        {
            throw std::runtime_error("Package " + package + " not found in shard index");
        }

        // Convert hash bytes to hex string
        std::string hex_hash = util::bytes_to_hex_str(
            reinterpret_cast<const std::byte*>(it->second.data()),
            reinterpret_cast<const std::byte*>(it->second.data() + it->second.size())
        );
        std::string shard_name = hex_hash + ".msgpack.zst";

        std::string shards_base_url_str = m_shards_index.info.shards_base_url;

        // Check if shards_base_url is absolute (has a URL scheme)
        bool is_absolute = util::url_has_scheme(shards_base_url_str);

        if (is_absolute)
        {
            // For absolute URLs, check if it's on the same host as the channel
            // Parse URLs to extract hosts and paths
            auto shards_url_parsed = util::URL::parse(shards_base_url_str);
            auto channel_url_parsed = util::URL::parse(m_url);

            if (shards_url_parsed.has_value() && channel_url_parsed.has_value())
            {
                const auto& shards_url = shards_url_parsed.value();
                const auto& channel_url = channel_url_parsed.value();

                // Compare hosts (decoded)
                std::string shards_host = shards_url.host();
                std::string channel_host = channel_url.host();

                // If hosts match, extract path component relative to channel
                if (shards_host == channel_host)
                {
                    // Get path from shards URL (decoded, starts with '/')
                    std::string path = shards_url.path();
                    // Remove leading '/' since we want relative path
                    if (util::starts_with(path, "/"))
                    {
                        path = path.substr(1);
                    }

                    // Ensure trailing slash
                    if (!path.empty() && !util::ends_with(path, "/"))
                    {
                        path += "/";
                    }
                    return path + shard_name;
                }
            }
            // If hosts don't match or parsing failed, we can't use mirror system - return full URL
            // This will need special handling in fetch_shards
            // Use url_concat to ensure proper URL joining
            return util::url_concat(shards_base_url_str, shard_name);
        }
        else
        {
            // For relative URLs, construct path relative to platform
            // m_url is like: https://prefix.dev/conda-forge/linux-64/repodata_shards.msgpack.zst
            // Extract platform (subdir) from m_url or use from index
            std::string platform = m_shards_index.info.subdir;

            // Normalize shards_base_url_str (remove ./ prefix if present)
            std::string normalized_shards = shards_base_url_str;
            if (util::starts_with(normalized_shards, "./"))
            {
                normalized_shards = normalized_shards.substr(2);
            }
            if (util::starts_with(normalized_shards, "/"))
            {
                normalized_shards = normalized_shards.substr(1);
            }

            // Construct path: platform/shards/<hash>.msgpack.zst
            // url_concat handles slashes automatically, no need for "/" separator
            std::string path = util::url_concat(platform, normalized_shards);
            if (!util::ends_with(path, "/"))
            {
                path += "/";
            }
            return path + shard_name;
        }
    }

    auto Shards::is_shard_present(const std::string& package) const -> bool
    {
        return m_visited.find(package) != m_visited.end();
    }

    auto Shards::visit_package(const std::string& package) const -> ShardDict
    {
        auto it = m_visited.find(package);
        if (it == m_visited.end())
        {
            throw std::runtime_error("Package " + package + " shard not visited");
        }
        return it->second;
    }

    void Shards::process_fetched_shard(const std::string& package, const ShardDict& shard)
    {
        // Log dependencies found in the shard (only if DEBUG or TRACE level is enabled)
        if (logging::get_log_level() <= log_level::debug)
        {
            LOG_DEBUG << "Processing fetched shard for package '" << package
                      << "': " << shard.packages.size() << " .tar.bz2 packages, "
                      << shard.conda_packages.size() << " .conda packages";

            for (const auto& [filename, record] : shard.packages)
            {
                if (!record.depends.empty())
                {
                    LOG_DEBUG << "Package '" << record.name << "' from shard '" << package
                              << "' has dependencies: [" << util::join(", ", record.depends) << "]";
                }
            }
            for (const auto& [filename, record] : shard.conda_packages)
            {
                if (!record.depends.empty())
                {
                    LOG_DEBUG << "Conda package '" << record.name << "' from shard '" << package
                              << "' has dependencies: [" << util::join(", ", record.depends) << "]";
                }
            }
        }

        m_visited[package] = shard;
    }

    auto Shards::fetch_shard(const std::string& package) -> expected_t<ShardDict>
    {
        std::vector<std::string> packages = { package };
        auto result = fetch_shards(packages);
        if (!result.has_value())
        {
            return make_unexpected(result.error().what(), result.error().error_code());
        }
        auto it = result.value().find(package);
        if (it == result.value().end())
        {
            return make_unexpected(
                "Package " + package + " not found after fetch",
                mamba_error_code::unknown
            );
        }
        return it->second;
    }

    void Shards::filter_packages_to_fetch(
        const std::vector<std::string>& packages,
        std::map<std::string, ShardDict>& results,
        std::vector<std::string>& packages_to_fetch
    ) const
    {
        for (const auto& package : packages)
        {
            if (auto it = m_visited.find(package); it != m_visited.end())
            {
                LOG_DEBUG << "Shard for package '" << package
                          << "' already in memory, skipping download";
                results[package] = it->second;
            }
            else
            {
                LOG_DEBUG << "Shard for package '" << package << "' needs to be downloaded";
                packages_to_fetch.push_back(package);
            }
        }
    }

    void Shards::build_shard_urls(
        const std::vector<std::string>& packages_to_fetch,
        std::vector<std::string>& urls,
        std::map<std::string, std::string>& url_to_package
    ) const
    {
        LOG_DEBUG << "Building shard URLs for " << packages_to_fetch.size()
                  << " package(s) from base_url: " << shards_base_url();
        for (const auto& package : packages_to_fetch)
        {
            try
            {
                std::string url = shard_url(package);
                LOG_DEBUG << "Built shard URL for package '" << package << "': " << url;
                urls.push_back(url);
                url_to_package[url] = package;
            }
            catch (const std::exception& e)
            {
                LOG_WARNING << "Failed to get shard URL for " << package << ": " << e.what();
            }
        }
        LOG_DEBUG << "Successfully built " << urls.size() << " shard URL(s) out of "
                  << packages_to_fetch.size() << " package(s)";
    }

    void Shards::create_download_requests(
        const std::map<std::string, std::string>& url_to_package,
        const std::string& cache_dir_str,
        download::mirror_map& extended_mirrors,
        download::MultiRequest& requests,
        std::vector<std::string>& cache_miss_urls,
        std::vector<std::string>& cache_miss_packages,
        std::map<std::string, fs::u8path>& package_to_artifact_path,
        std::vector<std::shared_ptr<TemporaryFile>>& artifacts
    ) const
    {
        // Reserve space in the vectors
        const std::size_t num_file_to_fetch = url_to_package.size();

        cache_miss_urls.reserve(num_file_to_fetch);
        cache_miss_packages.reserve(num_file_to_fetch);
        artifacts.reserve(num_file_to_fetch);

        for (const auto& [url, package] : url_to_package)
        {
            cache_miss_urls.push_back(url);
            cache_miss_packages.push_back(package);

            auto artifact = std::make_shared<TemporaryFile>("mambashard", "", cache_dir_str);
            artifacts.push_back(artifact);

            fs::u8path artifact_path = artifact->path();
            package_to_artifact_path[package] = artifact_path;

            std::string shard_path_str = relative_shard_path(package);
            bool is_full_url = util::url_has_scheme(shard_path_str);

            std::string mirror_name;
            std::string url_path;

            if (is_full_url)
            {
                // Parse URL to extract base URL and relative path
                auto url_parsed = util::URL::parse(shard_path_str);
                if (url_parsed.has_value())
                {
                    const auto& parsed_url = url_parsed.value();
                    // Construct base URL: scheme://host/
                    std::string base_url = util::url_concat(
                        parsed_url.scheme(),
                        "://",
                        parsed_url.host(),
                        "/"
                    );
                    // Get relative path (remove leading '/')
                    std::string path = parsed_url.path();
                    if (path.size() > 1)
                    {
                        url_path = path.substr(1);  // Remove leading '/'
                    }
                    else
                    {
                        url_path = "";
                    }

                    mirror_name = base_url;

                    if (!extended_mirrors.has_mirrors(mirror_name))
                    {
                        auto mirror = download::make_mirror(base_url);
                        if (mirror)
                        {
                            extended_mirrors.add_unique_mirror(mirror_name, std::move(mirror));
                        }
                    }
                }
                else
                {
                    // Parsing failed, use PassThroughMirror with full URL
                    mirror_name = "";
                    url_path = shard_path_str;
                }
            }
            else
            {
                mirror_name = m_channel.id();
                url_path = shard_path_str;
            }

            download::Request request(
                package + "-shard",
                download::MirrorName(mirror_name),
                url_path,
                artifact->path().string(),
                /*head_only*/ false,
                /*ignore_failure*/ false
            );

            request.on_success = [url, package](
                                     const download::Success& /* success */
                                 )
            {
                LOG_DEBUG << "Successfully fetched shard " << url << " for package " << package;
                return expected_t<void>();
            };

            request.on_failure = [url, package](const download::Error& error)
            {
                LOG_WARNING << "Failed to fetch shard " << url << " for package " << package << ": "
                            << error.message;
            };

            requests.push_back(std::move(request));
        }
    }

    auto Shards::decompress_zstd_shard(const std::vector<std::uint8_t>& compressed_data)
        -> expected_t<std::vector<std::uint8_t>>
    {
        LOG_DEBUG << "Decompressing shard using zstd";
        ZSTD_DCtx* dctx = ZSTD_createDCtx();
        if (dctx == nullptr)
        {
            return make_unexpected(
                "Failed to create zstd decompression context",
                mamba_error_code::unknown
            );
        }

        std::vector<std::uint8_t> decompressed_data(ZSTD_DStreamOutSize());
        ZSTD_inBuffer input = { compressed_data.data(), compressed_data.size(), 0 };
        ZSTD_outBuffer output = { decompressed_data.data(), decompressed_data.size(), 0 };

        std::vector<std::uint8_t> full_decompressed;
        while (input.pos < input.size)
        {
            std::size_t ret = ZSTD_decompressStream(dctx, &output, &input);
            if (ZSTD_isError(ret))
            {
                ZSTD_freeDCtx(dctx);
                return make_unexpected(
                    std::string("Zstd decompression error: ") + ZSTD_getErrorName(ret),
                    mamba_error_code::unknown
                );
            }

            full_decompressed.insert(
                full_decompressed.end(),
                decompressed_data.begin(),
                decompressed_data.begin() + static_cast<std::ptrdiff_t>(output.pos)
            );
            output.pos = 0;

            constexpr std::size_t MAX_SHARD_SIZE = 100 * 1024 * 1024;
            if (full_decompressed.size() > MAX_SHARD_SIZE)
            {
                ZSTD_freeDCtx(dctx);
                return make_unexpected(
                    "Decompressed shard exceeds maximum size",
                    mamba_error_code::unknown
                );
            }
        }

        ZSTD_freeDCtx(dctx);
        LOG_DEBUG << "Decompressed shard: " << compressed_data.size() << " bytes -> "
                  << full_decompressed.size() << " bytes";
        return full_decompressed;
    }

    auto Shards::parse_shard_msgpack(
        const std::vector<std::uint8_t>& decompressed_data,
        const std::string& package
    ) -> expected_t<ShardDict>
    {
        LOG_DEBUG << "Parsing msgpack data for package '" << package << "' shard";
        msgpack_unpacked unpacked = {};
        try
        {
            size_t offset = 0;
            msgpack_unpack_return ret = msgpack_unpack_next(
                &unpacked,
                reinterpret_cast<const char*>(decompressed_data.data()),
                decompressed_data.size(),
                &offset
            );

            if (ret != MSGPACK_UNPACK_SUCCESS && ret != MSGPACK_UNPACK_EXTRA_BYTES)
            {
                if (unpacked.zone != nullptr)
                {
                    msgpack_zone_destroy(unpacked.zone);
                    unpacked.zone = nullptr;
                }
                return make_unexpected("Failed to unpack msgpack data", mamba_error_code::unknown);
            }

            const msgpack_object& obj = unpacked.data;
            ShardDict shard;

            auto parse_package_records = [&package](
                                             const msgpack_object& map_obj,
                                             std::map<std::string, ShardPackageRecord>& target_map,
                                             const std::string& log_prefix,
                                             const std::string& map_name
                                         )
            {
                for (std::uint32_t k = 0; k < map_obj.via.map.size; ++k)
                {
                    try
                    {
                        std::string pkg_filename = msgpack_object_to_string(map_obj.via.map.ptr[k].key);
                        ShardPackageRecord record = parse_shard_package_record(
                            map_obj.via.map.ptr[k].val
                        );
                        LOG_DEBUG << "Parsed " << log_prefix
                                  << " package record from shard for package '" << package
                                  << "': " << record.name << "=" << record.version << "-"
                                  << record.build;
                        target_map[pkg_filename] = record;
                    }
                    catch (const std::exception& e)
                    {
                        LOG_WARNING << "Failed to parse package record in '" << map_name
                                    << "': " << e.what();
                    }
                }
            };

            if (obj.type != MSGPACK_OBJECT_MAP)
            {
                return make_unexpected(
                    "Expected MAP type for shard msgpack, but got " + std::to_string(obj.type),
                    mamba_error_code::unknown
                );
            }

            for (std::uint32_t j = 0; j < obj.via.map.size; ++j)
            {
                const msgpack_object& key_obj = obj.via.map.ptr[j].key;
                const msgpack_object& val_obj = obj.via.map.ptr[j].val;

                std::string key;
                try
                {
                    key = msgpack_object_to_string(key_obj);
                }
                catch (const std::exception&)
                {
                    LOG_WARNING << "Failed to parse msgpack object key at index " << j;
                    continue;
                }

                try
                {
                    if (key == "packages")
                    {
                        parse_package_records(val_obj, shard.packages, "package", "packages");
                    }
                    else if (key == "packages.conda")
                    {
                        parse_package_records(
                            val_obj,
                            shard.conda_packages,
                            "conda package",
                            "packages.conda"
                        );
                    }
                }
                catch (const std::exception& e)
                {
                    LOG_DEBUG << "Failed to parse field '" << key << "' in shard: " << e.what();
                }
            }

            if (unpacked.zone != nullptr)
            {
                msgpack_zone_destroy(unpacked.zone);
                unpacked.zone = nullptr;
            }

            LOG_DEBUG << "Successfully parsed shard for package '" << package
                      << "': " << shard.packages.size() << " .tar.bz2 packages, "
                      << shard.conda_packages.size() << " .conda packages";
            return shard;
        }
        catch (const std::exception& e)
        {
            if (unpacked.zone != nullptr)
            {
                msgpack_zone_destroy(unpacked.zone);
                unpacked.zone = nullptr;
            }
            return make_unexpected(
                std::string("Failed to parse msgpack: ") + e.what(),
                mamba_error_code::unknown
            );
        }
    }

    auto Shards::process_downloaded_shard(
        const std::string& package,
        const download::Success& success,
        const std::map<std::string, fs::u8path>& package_to_artifact_path
    ) -> expected_t<ShardDict>
    {
        fs::u8path shard_file;
        auto artifact_it = package_to_artifact_path.find(package);
        if (artifact_it != package_to_artifact_path.end())
        {
            shard_file = artifact_it->second;
            LOG_DEBUG << "Using artifact path for shard: " << shard_file.string();
        }
        else
        {
            return make_unexpected(
                "No artifact path found for package: " + package,
                mamba_error_code::unknown
            );
        }

        if (std::holds_alternative<download::Filename>(success.content))
        {
            std::string filename_value = std::get<download::Filename>(success.content).value;
            LOG_DEBUG << "Download returned Filename: " << filename_value;

            shard_file = filename_value;
            LOG_DEBUG << "Using filename from download result: " << shard_file.string();

            if (!fs::exists(shard_file))
            {
                return make_unexpected(
                    "Shard file not found at path: " + shard_file.string(),
                    mamba_error_code::unknown
                );
            }
        }
        else  // std::holds_alternative<download::Buffer>(success.content)
        {
            LOG_DEBUG << "Shard download returned buffer, writing to artifact path";
            const auto& buffer = std::get<download::Buffer>(success.content);
            std::ofstream out_file(shard_file.string(), std::ios::binary);
            if (!out_file.is_open())
            {
                return make_unexpected(
                    "Failed to create shard file for buffer: " + shard_file.string(),
                    mamba_error_code::unknown
                );
            }
            out_file.write(buffer.value.data(), static_cast<std::streamsize>(buffer.value.size()));
            out_file.close();
            LOG_DEBUG << "Wrote buffer to file: " << shard_file.string() << " ("
                      << buffer.value.size() << " bytes)";
        }

        std::ifstream file(shard_file.string(), std::ios::binary);
        if (!file.is_open())
        {
            return make_unexpected(
                "Failed to open downloaded shard file: " + shard_file.string(),
                mamba_error_code::unknown
            );
        }

        std::vector<std::uint8_t> compressed_data(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        file.close();

        LOG_DEBUG << "Reading shard file for package '" << package << "': " << shard_file.string()
                  << " (" << compressed_data.size() << " bytes compressed)";

        auto decompressed_result = decompress_zstd_shard(compressed_data);
        if (!decompressed_result.has_value())
        {
            return make_unexpected(
                decompressed_result.error().what(),
                decompressed_result.error().error_code()
            );
        }

        auto parse_result = parse_shard_msgpack(decompressed_result.value(), package);
        if (!parse_result.has_value())
        {
            return make_unexpected(parse_result.error().what(), parse_result.error().error_code());
        }

        return parse_result.value();
    }

    auto Shards::fetch_shards(const std::vector<std::string>& packages)
        -> expected_t<std::map<std::string, ShardDict>>
    {
        std::map<std::string, ShardDict> results;
        std::vector<std::string> packages_to_fetch;

        filter_packages_to_fetch(packages, results, packages_to_fetch);

        if (packages_to_fetch.empty())
        {
            return results;
        }

        std::vector<std::string> urls;
        std::map<std::string, std::string> url_to_package;
        build_shard_urls(packages_to_fetch, urls, url_to_package);

        if (url_to_package.empty())
        {
            return results;
        }

        download::MultiRequest requests;
        std::vector<std::string> cache_miss_urls;
        std::vector<std::string> cache_miss_packages;
        std::map<std::string, fs::u8path> package_to_artifact_path;
        std::vector<std::shared_ptr<TemporaryFile>> artifacts;
        download::mirror_map extended_mirrors;

        const bool XDG_CACHE_HOME_SET = util::get_env("XDG_CACHE_HOME").has_value();
        const fs::u8path cache_dir_path = fs::u8path(
                                              XDG_CACHE_HOME_SET
                                                  ? util::get_env("XDG_CACHE_HOME").value()
                                                  : util::user_cache_dir()
                                          )
                                          / "conda" / "pkgs";
        const std::string cache_dir_str = create_cache_dir(cache_dir_path);

        create_download_requests(
            url_to_package,
            cache_dir_str,
            extended_mirrors,
            requests,
            cache_miss_urls,
            cache_miss_packages,
            package_to_artifact_path,
            artifacts
        );

        LOG_DEBUG << "Downloading " << requests.size() << " shard(s) for packages: ["
                  << util::join(", ", packages_to_fetch) << "]";
        download::Options download_options;
        download_options.download_threads = m_download_threads;

        auto download_results = download::download(
            std::move(requests),
            extended_mirrors,
            m_remote_fetch_params,
            m_auth_info,
            download_options,
            nullptr  // monitor
        );

        for (std::size_t i = 0; i < download_results.size() && i < cache_miss_urls.size(); ++i)
        {
            const std::string& url = cache_miss_urls[i];
            const std::string& package = cache_miss_packages[i];

            if (!download_results[i].has_value())
            {
                LOG_WARNING << "Failed to download shard " << url << " for package '" << package
                            << "'";
                continue;
            }

            const auto& success = download_results[i].value();
            LOG_DEBUG << "Successfully downloaded shard for package '" << package << "' from "
                      << url << " (" << success.transfer.downloaded_size << " bytes)";

            auto shard_result = process_downloaded_shard(package, success, package_to_artifact_path);
            if (!shard_result.has_value())
            {
                LOG_WARNING << "Failed to process downloaded shard for package '" << package
                            << "': " << shard_result.error().what();
                continue;
            }

            results[package] = shard_result.value();
            process_fetched_shard(package, shard_result.value());
        }

        return results;
    }

    auto Shards::build_repodata() const -> RepodataDict
    {
        RepodataDict repodata;
        repodata.info = m_shards_index.info;
        repodata.repodata_version = 2;

        // Collect all packages first, then sort by version and build number
        // This ensures that when libsolv processes packages, it sees them in
        // the correct order (highest version/build first)
        std::vector<std::pair<std::string, ShardPackageRecord>> all_packages;
        std::vector<std::pair<std::string, ShardPackageRecord>> all_conda_packages;

        for (const auto& [package, shard] : m_visited)
        {
            // Collect packages
            for (const auto& [filename, record] : shard.packages)
            {
                all_packages.emplace_back(filename, record);
            }
            // Collect conda packages
            for (const auto& [filename, record] : shard.conda_packages)
            {
                all_conda_packages.emplace_back(filename, record);
            }
        }

        // Sort packages by name, then by version (descending), then by build number (descending)
        // This ensures that when multiple versions exist, the highest version/build is processed
        // first
        auto compare_packages = [](const auto& a, const auto& b)
        {
            const auto& record_a = a.second;
            const auto& record_b = b.second;

            // First compare by package name
            if (record_a.name != record_b.name)
            {
                return record_a.name < record_b.name;
            }

            // Then compare by version (descending - highest first)
            // Parse versions for comparison
            auto version_a = specs::Version::parse(record_a.version);
            auto version_b = specs::Version::parse(record_b.version);

            if (version_a.has_value() && version_b.has_value())
            {
                if (version_a.value() != version_b.value())
                {
                    return version_b.value() < version_a.value();  // Descending order
                }
            }
            else if (version_a.has_value() || version_b.has_value())
            {
                // If only one can be parsed, prefer the parsed one
                return !version_a.has_value();
            }
            else
            {
                // Fallback to string comparison if parsing fails
                if (record_a.version != record_b.version)
                {
                    return record_b.version < record_a.version;  // Descending order
                }
            }

            // Finally compare by build number (descending - highest first)
            if (record_a.build_number != record_b.build_number)
            {
                return record_b.build_number < record_a.build_number;  // Descending order
            }

            // If everything else is equal, compare by build string
            return record_b.build < record_a.build;  // Descending order
        };

        std::sort(all_packages.begin(), all_packages.end(), compare_packages);
        std::sort(all_conda_packages.begin(), all_conda_packages.end(), compare_packages);

        // Insert sorted packages into repodata
        for (const auto& [filename, record] : all_packages)
        {
            repodata.shard_dict.packages[filename] = record;
        }
        for (const auto& [filename, record] : all_conda_packages)
        {
            repodata.shard_dict.conda_packages[filename] = record;
        }

        return repodata;
    }

    auto Shards::base_url() const -> std::string
    {
        return m_shards_index.info.base_url;
    }

    auto Shards::url() const -> std::string
    {
        return m_url;
    }
}
