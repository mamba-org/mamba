// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>

#include <msgpack.h>
#include <msgpack/zone.h>
#include <zstd.h>

#include "mamba/core/logging.hpp"
#include "mamba/core/shard_index.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/util.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{
    auto ShardIndexLoader::shard_index_cache_path(const SubdirIndexLoader& subdir) -> fs::u8path
    {
        // Use similar naming as repodata.json cache but with .msgpack.zst extension
        std::string cache_name = cache_filename_from_url(subdir.name());
        // Replace .json extension with .msgpack.zst
        if (util::ends_with(cache_name, ".json"))
        {
            cache_name = cache_name.substr(0, cache_name.size() - 5) + ".msgpack.zst";
        }
        else
        {
            cache_name += ".msgpack.zst";
        }
        return subdir.writable_libsolv_cache_path().parent_path() / cache_name;
    }

    auto ShardIndexLoader::build_shard_index_request(
        const SubdirIndexLoader& subdir,
        const SubdirDownloadParams& params,
        const fs::u8path& cache_dir
    ) -> std::optional<download::Request>
    {
        if (params.offline)
        {
            return std::nullopt;
        }

        // Check if shards are available (must be true to fetch shard index)
        // If has_up_to_date_shards() returns false, it means shards are not available
        // and we should not try to fetch the index
        if (!subdir.metadata().has_up_to_date_shards())
        {
            // Shards are not available (check request returned 404 or hasn't completed)
            return std::nullopt;
        }

        fs::u8path writable_cache_dir = create_cache_dir(cache_dir);
        auto lock = LockFile(writable_cache_dir);

        auto artifact = std::make_shared<TemporaryFile>("mambashard", "", writable_cache_dir);

        // Build path for repodata_shards.msgpack.zst
        // Construct path component manually (mirror system will prepend base URL)
        // Format: platform/repodata_shards.msgpack.zst
        std::string platform_str = subdir.platform();
        std::string shard_index_path = util::url_concat(platform_str, "/repodata_shards.msgpack.zst");
        std::string shard_index_url = shard_index_path;

        download::Request request(
            subdir.name() + "-shards",
            download::MirrorName(subdir.channel_id()),
            shard_index_url,
            artifact->path().string(),
            /*head_only*/ false,
            /*ignore_failure*/ !subdir.is_noarch()
        );

        // Use HTTP caching if available
        request.etag = subdir.metadata().etag();
        request.last_modified = subdir.metadata().last_modified();

        request.on_success = [](const download::Success& /* success */)
        {
            // Success callback - the file path is in success.content
            return expected_t<void>();
        };

        request.on_failure = [](const download::Error& error)
        {
            if (error.transfer.has_value())
            {
                LOG_DEBUG << "Shard index not available (response: "
                          << error.transfer.value().http_status << ") for '"
                          << error.transfer.value().effective_url << "'";
            }
            else
            {
                LOG_DEBUG << "Failed to fetch shard index: " << error.message;
            }
        };

        return { std::move(request) };
    }

    auto ShardIndexLoader::parse_shard_index(const fs::u8path& file_path)
        -> expected_t<ShardsIndexDict>
    {
        if (!fs::exists(file_path))
        {
            return make_unexpected("Shard index file does not exist", mamba_error_code::cache_not_loaded);
        }

        // Read compressed data
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open())
        {
            return make_unexpected("Failed to open shard index file", mamba_error_code::cache_not_loaded);
        }

        std::vector<std::uint8_t> compressed_data(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        file.close();

        if (compressed_data.empty())
        {
            return make_unexpected("Shard index file is empty", mamba_error_code::cache_not_loaded);
        }

        // Decompress zstd
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
                    "Zstd decompression error: " + std::string(ZSTD_getErrorName(ret)),
                    mamba_error_code::unknown
                );
            }

            full_decompressed.insert(
                full_decompressed.end(),
                decompressed_data.begin(),
                decompressed_data.begin() + static_cast<std::ptrdiff_t>(output.pos)
            );
            output.pos = 0;
            // Reasonable limit for shard index (should be much smaller than individual shards)
            if (full_decompressed.size() > 100 * 1024 * 1024)  // 100 MB
            {
                ZSTD_freeDCtx(dctx);
                return make_unexpected(
                    "Decompressed shard index exceeds maximum size",
                    mamba_error_code::unknown
                );
            }
        }

        ZSTD_freeDCtx(dctx);

        // Parse msgpack
        try
        {
            msgpack_unpacked unpacked;
            size_t offset = 0;
            msgpack_unpack_return ret = msgpack_unpack_next(
                &unpacked,
                reinterpret_cast<const char*>(full_decompressed.data()),
                full_decompressed.size(),
                &offset
            );

            if (ret != MSGPACK_UNPACK_SUCCESS && ret != MSGPACK_UNPACK_EXTRA_BYTES)
            {
                throw std::runtime_error("Failed to unpack msgpack data");
            }

            const msgpack_object& obj = unpacked.data;

            // Handle both 'version' and 'repodata_version' field names (conda-index uses
            // 'repodata_version')
            ShardsIndexDict index;

            // Manual parsing to handle field name variations
            if (obj.type == MSGPACK_OBJECT_MAP)
            {
                bool has_info = false;
                bool has_version = false;
                bool has_shards = false;

                for (std::uint32_t i = 0; i < obj.via.map.size; ++i)
                {
                    const msgpack_object& key_obj = obj.via.map.ptr[i].key;
                    const msgpack_object& val_obj = obj.via.map.ptr[i].val;

                    std::string key;
                    try
                    {
                        if (key_obj.type == MSGPACK_OBJECT_STR)
                        {
                            key = std::string(key_obj.via.str.ptr, key_obj.via.str.size);
                        }
                        else if (key_obj.type == MSGPACK_OBJECT_BIN)
                        {
                            key = std::string(key_obj.via.bin.ptr, key_obj.via.bin.size);
                        }
                        else
                        {
                            continue;
                        }
                    }
                    catch (const std::exception&)
                    {
                        // Skip if key conversion fails
                        continue;
                    }

                    try
                    {
                        if (key == "info")
                        {
                            // Manually parse RepodataInfoDict to handle structure variations
                            if (val_obj.type == MSGPACK_OBJECT_MAP)
                            {
                                RepodataInfoDict info_dict;
                                for (std::uint32_t j = 0; j < val_obj.via.map.size; ++j)
                                {
                                    const msgpack_object& info_key_obj = val_obj.via.map.ptr[j].key;
                                    const msgpack_object& info_val_obj = val_obj.via.map.ptr[j].val;

                                    std::string info_key;
                                    if (info_key_obj.type == MSGPACK_OBJECT_STR)
                                    {
                                        info_key = std::string(
                                            info_key_obj.via.str.ptr,
                                            info_key_obj.via.str.size
                                        );
                                    }
                                    else if (info_key_obj.type == MSGPACK_OBJECT_BIN)
                                    {
                                        info_key = std::string(
                                            info_key_obj.via.bin.ptr,
                                            info_key_obj.via.bin.size
                                        );
                                    }
                                    else
                                    {
                                        continue;
                                    }

                                    if (info_key == "base_url")
                                    {
                                        info_dict.base_url = std::string(
                                            info_val_obj.via.str.ptr,
                                            info_val_obj.via.str.size
                                        );
                                    }
                                    else if (info_key == "shards_base_url")
                                    {
                                        info_dict.shards_base_url = std::string(
                                            info_val_obj.via.str.ptr,
                                            info_val_obj.via.str.size
                                        );
                                    }
                                    else if (info_key == "subdir")
                                    {
                                        info_dict.subdir = std::string(
                                            info_val_obj.via.str.ptr,
                                            info_val_obj.via.str.size
                                        );
                                    }
                                }
                                index.info = info_dict;
                                has_info = true;
                            }
                        }
                        else if (key == "version" || key == "repodata_version")
                        {
                            // Handle both field names (conda-index uses 'repodata_version')
                            // Try different integer types
                            if (val_obj.type == MSGPACK_OBJECT_POSITIVE_INTEGER)
                            {
                                index.version = static_cast<std::size_t>(val_obj.via.u64);
                            }
                            else if (val_obj.type == MSGPACK_OBJECT_NEGATIVE_INTEGER)
                            {
                                index.version = static_cast<std::size_t>(val_obj.via.i64);
                            }
                            has_version = true;
                        }
                        else if (key == "shards")
                        {
                            // Parse shards map: package name -> hash bytes
                            if (val_obj.type == MSGPACK_OBJECT_MAP)
                            {
                                for (std::uint32_t j = 0; j < val_obj.via.map.size; ++j)
                                {
                                    const msgpack_object& shard_key_obj = val_obj.via.map.ptr[j].key;
                                    const msgpack_object& shard_val_obj = val_obj.via.map.ptr[j].val;

                                    std::string package_name;
                                    if (shard_key_obj.type == MSGPACK_OBJECT_STR)
                                    {
                                        package_name = std::string(
                                            shard_key_obj.via.str.ptr,
                                            shard_key_obj.via.str.size
                                        );
                                    }
                                    else if (shard_key_obj.type == MSGPACK_OBJECT_BIN)
                                    {
                                        package_name = std::string(
                                            shard_key_obj.via.bin.ptr,
                                            shard_key_obj.via.bin.size
                                        );
                                    }
                                    else
                                    {
                                        continue;
                                    }

                                    // Hash is stored as binary data
                                    if (shard_val_obj.type == MSGPACK_OBJECT_BIN)
                                    {
                                        std::vector<std::uint8_t> hash_bytes(
                                            reinterpret_cast<const std::uint8_t*>(
                                                shard_val_obj.via.bin.ptr
                                            ),
                                            reinterpret_cast<const std::uint8_t*>(
                                                shard_val_obj.via.bin.ptr + shard_val_obj.via.bin.size
                                            )
                                        );
                                        index.shards[package_name] = hash_bytes;
                                    }
                                    else if (shard_val_obj.type == MSGPACK_OBJECT_STR)
                                    {
                                        // Sometimes hash is stored as hex string
                                        std::string hex_str(
                                            shard_val_obj.via.str.ptr,
                                            shard_val_obj.via.str.size
                                        );
                                        // Convert hex string to bytes (simplified - would need
                                        // proper hex parsing)
                                        std::vector<std::uint8_t> hash_bytes;
                                        hash_bytes.reserve(hex_str.size() / 2);
                                        for (size_t k = 0; k < hex_str.size(); k += 2)
                                        {
                                            if (k + 1 < hex_str.size())
                                            {
                                                std::string byte_str = hex_str.substr(k, 2);
                                                hash_bytes.push_back(
                                                    static_cast<std::uint8_t>(
                                                        std::stoul(byte_str, nullptr, 16)
                                                    )
                                                );
                                            }
                                        }
                                        index.shards[package_name] = hash_bytes;
                                    }
                                }
                            }
                            has_shards = true;
                        }
                    }
                    catch (const std::exception& e)
                    {
                        LOG_WARNING << "Failed to parse field '" << key
                                    << "' (type=" << static_cast<int>(val_obj.type)
                                    << "): " << e.what();
                        // Continue with other fields
                    }
                }

                if (!has_info || !has_shards)
                {
                    LOG_WARNING << "Missing required fields in shard index (has_info=" << has_info
                                << ", has_version=" << has_version << ", has_shards=" << has_shards
                                << ")";
                }
            }

            msgpack_zone_destroy(unpacked.zone);
            return index;
        }
        catch (const std::exception& e)
        {
            return make_unexpected(
                "Failed to parse msgpack: " + std::string(e.what()),
                mamba_error_code::unknown
            );
        }
    }

    auto ShardIndexLoader::fetch_shards_index(
        const SubdirIndexLoader& subdir,
        const SubdirDownloadParams& params,
        const specs::AuthenticationDataBase& auth_info,
        const download::mirror_map& mirrors,
        const download::Options& download_options,
        const download::RemoteFetchParams& remote_fetch_params
    ) -> expected_t<std::optional<ShardsIndexDict>>
    {
        // Check cache first
        fs::u8path cache_path = shard_index_cache_path(subdir);
        if (fs::exists(cache_path))
        {
            auto cached_index = parse_shard_index(cache_path);
            if (cached_index.has_value())
            {
                LOG_DEBUG << "Using cached shard index for " << subdir.name();
                return std::optional<ShardsIndexDict>(std::move(cached_index.value()));
            }
        }

        // Build download request
        auto request_opt = build_shard_index_request(
            subdir,
            params,
            subdir.writable_libsolv_cache_path().parent_path()
        );
        if (!request_opt.has_value())
        {
            // Shards not available or offline mode
            return std::optional<ShardsIndexDict>{};
        }

        // Download
        download::MultiRequest requests;
        requests.push_back(*std::move(request_opt));

        try
        {
            auto results = download::download(
                std::move(requests),
                mirrors,
                remote_fetch_params,
                auth_info,
                download_options,
                nullptr  // monitor
            );

            if (results.empty() || !results[0].has_value())
            {
                // Download failed, shards not available
                return std::optional<ShardsIndexDict>{};
            }

            const auto& result = results[0].value();

            // Extract file path from download result
            fs::u8path downloaded_path;
            if (std::holds_alternative<download::Filename>(result.content))
            {
                downloaded_path = std::get<download::Filename>(result.content).value;
            }
            else
            {
                // Buffer download not supported for shard index
                return make_unexpected(
                    "Shard index download returned buffer instead of file",
                    mamba_error_code::unknown
                );
            }

            // Parse the downloaded file
            auto index_result = parse_shard_index(downloaded_path);
            if (!index_result.has_value())
            {
                return make_unexpected(index_result.error().what(), index_result.error().error_code());
            }

            // Cache the file if it's not already cached
            if (downloaded_path != cache_path)
            {
                fs::create_directories(cache_path.parent_path());
                fs::copy_file(downloaded_path, cache_path, fs::copy_options::overwrite_existing);
            }

            return std::optional<ShardsIndexDict>(std::move(index_result.value()));
        }
        catch (const std::exception& e)
        {
            LOG_DEBUG << "Exception while fetching shard index: " << e.what();
            return std::optional<ShardsIndexDict>{};
        }
    }
}
