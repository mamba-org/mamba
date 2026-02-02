// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include <fmt/format.h>
#include <msgpack.h>
#include <msgpack/zone.h>
#include <zstd.h>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/logging.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/shard_index_loader.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/util.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/encoding.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace
{
    /**
     * Parse the "shards" map from msgpack (package name -> hash bytes).
     */
    void
    parse_shards_map(const msgpack_object& val_obj, std::map<std::string, std::vector<std::uint8_t>>& shards)
    {
        if (val_obj.type != MSGPACK_OBJECT_MAP)
        {
            return;
        }
        for (std::uint32_t j = 0; j < val_obj.via.map.size; ++j)
        {
            const msgpack_object& shard_key_obj = val_obj.via.map.ptr[j].key;
            const msgpack_object& shard_val_obj = val_obj.via.map.ptr[j].val;

            std::string package_name;
            if (shard_key_obj.type == MSGPACK_OBJECT_STR)
            {
                package_name = std::string(shard_key_obj.via.str.ptr, shard_key_obj.via.str.size);
            }
            else if (shard_key_obj.type == MSGPACK_OBJECT_BIN)
            {
                package_name = std::string(shard_key_obj.via.bin.ptr, shard_key_obj.via.bin.size);
            }
            else
            {
                continue;
            }

            // Hash is stored as binary data
            if (shard_val_obj.type == MSGPACK_OBJECT_BIN)
            {
                std::vector<std::uint8_t> hash_bytes(
                    reinterpret_cast<const std::uint8_t*>(shard_val_obj.via.bin.ptr),
                    reinterpret_cast<const std::uint8_t*>(
                        shard_val_obj.via.bin.ptr + shard_val_obj.via.bin.size
                    )
                );
                shards[package_name] = std::move(hash_bytes);
            }
            else if (shard_val_obj.type == MSGPACK_OBJECT_STR)
            {
                // Sometimes hash is stored as hex string
                std::string_view hex_str(shard_val_obj.via.str.ptr, shard_val_obj.via.str.size);
                std::vector<std::uint8_t> hash_bytes(hex_str.size() / 2);
                mamba::util::EncodingError error = mamba::util::EncodingError::Ok;
                mamba::util::hex_to_bytes_to(
                    hex_str,
                    reinterpret_cast<std::byte*>(hash_bytes.data()),
                    error
                );
                if (error == mamba::util::EncodingError::Ok)
                {
                    shards[package_name] = std::move(hash_bytes);
                }
            }
        }
    }

    /**
     * Parse the "info" map from msgpack (RepoMetadata: base_url, shards_base_url, subdir).
     */
    std::optional<mamba::RepoMetadata> parse_info_map(const msgpack_object& val_obj)
    {
        if (val_obj.type != MSGPACK_OBJECT_MAP)
        {
            return std::nullopt;
        }
        mamba::RepoMetadata info_dict;
        for (std::uint32_t j = 0; j < val_obj.via.map.size; ++j)
        {
            const msgpack_object& info_key_obj = val_obj.via.map.ptr[j].key;
            const msgpack_object& info_val_obj = val_obj.via.map.ptr[j].val;

            std::string info_key;
            if (info_key_obj.type == MSGPACK_OBJECT_STR)
            {
                info_key = std::string(info_key_obj.via.str.ptr, info_key_obj.via.str.size);
            }
            else if (info_key_obj.type == MSGPACK_OBJECT_BIN)
            {
                info_key = std::string(info_key_obj.via.bin.ptr, info_key_obj.via.bin.size);
            }
            else
            {
                continue;
            }

            if (info_key == "base_url")
            {
                info_dict.base_url = std::string(info_val_obj.via.str.ptr, info_val_obj.via.str.size);
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
                info_dict.subdir = std::string(info_val_obj.via.str.ptr, info_val_obj.via.str.size);
            }
        }
        return info_dict;
    }

    /**
     * Read shard index file into a byte vector.
     */
    mamba::expected_t<std::vector<std::uint8_t>>
    read_shard_index_file(const mamba::fs::u8path& file_path)
    {
        std::ifstream file(file_path.string(), std::ios::binary);
        if (!file.is_open())
        {
            return mamba::make_unexpected(
                "Failed to open shard index file",
                mamba::mamba_error_code::cache_not_loaded
            );
        }
        std::vector<std::uint8_t> data(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        file.close();
        if (data.empty())
        {
            return mamba::make_unexpected(
                "Shard index file is empty",
                mamba::mamba_error_code::cache_not_loaded
            );
        }
        return data;
    }

    /**
     * Decompress zstd-compressed shard index data.
     */
    mamba::expected_t<std::vector<std::uint8_t>>
    decompress_shard_index_zstd(const std::vector<std::uint8_t>& compressed_data)
    {
        ZSTD_DCtx* dctx = ZSTD_createDCtx();
        if (dctx == nullptr)
        {
            return mamba::make_unexpected(
                "Failed to create zstd decompression context",
                mamba::mamba_error_code::unknown
            );
        }
        std::vector<std::uint8_t> decompressed_data(ZSTD_DStreamOutSize());
        ZSTD_inBuffer input = { compressed_data.data(), compressed_data.size(), 0 };
        ZSTD_outBuffer output = { decompressed_data.data(), decompressed_data.size(), 0 };
        std::vector<std::uint8_t> full_decompressed;
        constexpr std::size_t max_size = 100 * 1024 * 1024;  // 100 MB

        while (input.pos < input.size)
        {
            std::size_t ret = ZSTD_decompressStream(dctx, &output, &input);
            if (ZSTD_isError(ret))
            {
                ZSTD_freeDCtx(dctx);
                return mamba::make_unexpected(
                    "Zstd decompression error: " + std::string(ZSTD_getErrorName(ret)),
                    mamba::mamba_error_code::unknown
                );
            }
            full_decompressed.insert(
                full_decompressed.end(),
                decompressed_data.begin(),
                decompressed_data.begin() + static_cast<std::ptrdiff_t>(output.pos)
            );
            output.pos = 0;
            if (full_decompressed.size() > max_size)
            {
                ZSTD_freeDCtx(dctx);
                return mamba::make_unexpected(
                    "Decompressed shard index exceeds maximum size",
                    mamba::mamba_error_code::unknown
                );
            }
        }
        ZSTD_freeDCtx(dctx);
        return full_decompressed;
    }

    /**
     * Parse the root msgpack map into ShardsIndexDict (info, version, shards).
     */
    mamba::ShardsIndexDict parse_shard_index_map(const msgpack_object& obj)
    {
        mamba::ShardsIndexDict index;
        if (obj.type != MSGPACK_OBJECT_MAP)
        {
            return index;
        }
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
                continue;
            }

            try
            {
                if (key == "info")
                {
                    if (auto info_opt = parse_info_map(val_obj))
                    {
                        index.info = *info_opt;
                        has_info = true;
                    }
                }
                else if (key == "version" || key == "repodata_version")
                {
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
                    parse_shards_map(val_obj, index.shards);
                    has_shards = true;
                }
            }
            catch (const std::exception& e)
            {
                LOG_WARNING << "Failed to parse field '" << key
                            << "' (type=" << static_cast<int>(val_obj.type) << "): " << e.what();
            }
        }

        if (!has_info || !has_shards)
        {
            LOG_WARNING << "Missing required fields in shard index (has_info=" << has_info
                        << ", has_version=" << has_version << ", has_shards=" << has_shards << ")";
        }
        return index;
    }
}

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
        return subdir.writable_cache_dir() / cache_name;
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

        // Caller must have verified shards are available (e.g. via has_up_to_date_shards(ttl));
        // we only need availability here, so use has_shards().
        if (!subdir.metadata().has_shards())
        {
            return std::nullopt;
        }

        fs::u8path writable_cache_dir = create_cache_dir(cache_dir);
        // Lock the cache dir so concurrent downloads for the same subdir do not overwrite each
        // other; the temporary file path is then copied into the request and used as download
        // target.
        auto lock = LockFile(writable_cache_dir);

        // Unique path for this download; we copy to the final cache path only on success, so a
        // failed or partial download does not corrupt the cached shard index.
        auto artifact = std::make_shared<TemporaryFile>("mambashard", "", writable_cache_dir);

        download::Request request(
            subdir.name() + "-shards",
            download::MirrorName(subdir.channel_id()),
            subdir.shard_index_url_path(),
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

    auto ShardIndexLoader::build_shards_availability_check_request(SubdirIndexLoader& subdir)
        -> download::Request
    {
        download::Request request(
            subdir.name() + " (check shards)",
            download::MirrorName(subdir.channel_id()),
            subdir.shard_index_url_path(),
            "",
            /*head_only*/ true,
            /*ignore_failure*/ true
        );

        request.on_success = [&subdir](const download::Success& success)
        {
            int http_status = success.transfer.http_status;
            LOG_DEBUG << "Shard index check: " << success.transfer.effective_url << " ["
                      << http_status << "]";
            subdir.set_shards_availability(http_status >= 200 && http_status < 300);
            return expected_t<void>();
        };

        request.on_failure = [&subdir](const download::Error& error)
        {
            if (error.transfer.has_value())
            {
                LOG_DEBUG << "Shard index check failed: " << error.transfer.value().effective_url
                          << " [" << error.transfer.value().http_status << "]";
            }
            subdir.set_shards_availability(false);
        };

        return request;
    }

    void ShardIndexLoader::refresh_shards_availability(
        SubdirIndexLoader& subdir,
        const SubdirDownloadParams& params,
        const specs::AuthenticationDataBase& auth_info,
        const download::mirror_map& mirrors,
        const download::Options& download_options,
        const download::RemoteFetchParams& remote_fetch_params
    )
    {
        // Skip HEAD check only when offline and cache is allowed (same condition as
        // SubdirIndexLoader::build_check_requests for adding the shards check).
        if (params.offline && !subdir.caching_is_forbidden())
        {
            return;
        }

        download::MultiRequest requests;
        requests.push_back(build_shards_availability_check_request(subdir));

        download::download(
            std::move(requests),
            mirrors,
            remote_fetch_params,
            auth_info,
            download_options,
            nullptr  // monitor
        );
    }

    auto ShardIndexLoader::parse_shard_index(const fs::u8path& file_path)
        -> expected_t<ShardsIndexDict>
    {
        auto compressed = read_shard_index_file(file_path);
        if (!compressed.has_value())
        {
            return make_unexpected(compressed.error().what(), compressed.error().error_code());
        }

        auto decompressed = decompress_shard_index_zstd(compressed.value());
        if (!decompressed.has_value())
        {
            return make_unexpected(decompressed.error().what(), decompressed.error().error_code());
        }

        msgpack_unpacked unpacked = {};
        try
        {
            size_t offset = 0;
            msgpack_unpack_return ret = msgpack_unpack_next(
                &unpacked,
                reinterpret_cast<const char*>(decompressed->data()),
                decompressed->size(),
                &offset
            );

            if (ret != MSGPACK_UNPACK_SUCCESS && ret != MSGPACK_UNPACK_EXTRA_BYTES)
            {
                if (unpacked.zone != nullptr)
                {
                    msgpack_zone_destroy(unpacked.zone);
                    unpacked.zone = nullptr;
                }
                throw std::runtime_error("Failed to unpack msgpack data");
            }

            ShardsIndexDict index = parse_shard_index_map(unpacked.data);

            if (unpacked.zone != nullptr)
            {
                msgpack_zone_destroy(unpacked.zone);
                unpacked.zone = nullptr;
            }
            return index;
        }
        catch (const std::exception& e)
        {
            if (unpacked.zone != nullptr)
            {
                msgpack_zone_destroy(unpacked.zone);
                unpacked.zone = nullptr;
            }
            return make_unexpected(
                "Failed to parse msgpack: " + std::string(e.what()),
                mamba_error_code::unknown
            );
        }
    }

    auto ShardIndexLoader::fetch_and_parse_shard_index(
        SubdirIndexLoader& subdir,
        const SubdirDownloadParams& params,
        const specs::AuthenticationDataBase& auth_info,
        const download::mirror_map& mirrors,
        const download::Options& download_options,
        const download::RemoteFetchParams& remote_fetch_params,
        std::size_t shards_ttl
    ) -> expected_t<std::optional<ShardsIndexDict>>
    {
        // Refresh shards availability (HEAD check) if metadata is stale for TTL, then re-check.
        if (!subdir.metadata().has_up_to_date_shards(shards_ttl))
        {
            LOG_DEBUG << "Shard index metadata stale or missing for " << subdir.name()
                      << ", running HEAD check";
            refresh_shards_availability(
                subdir,
                params,
                auth_info,
                mirrors,
                download_options,
                remote_fetch_params
            );
            if (!subdir.metadata().has_up_to_date_shards(shards_ttl))
            {
                if (!subdir.metadata().has_shards())
                {
                    LOG_DEBUG << "Shards not present for " << subdir.name()
                              << " (HEAD check returned non-2xx status or failed)";
                }
                else
                {
                    LOG_DEBUG << "Shard index still not up to date for " << subdir.name()
                              << " after refresh (TTL or check failure)";
                }
                return std::nullopt;
            }
        }

        // Check cache first
        fs::u8path cache_path = shard_index_cache_path(subdir);
        if (fs::exists(cache_path))
        {
            auto cached_index = parse_shard_index(cache_path);
            if (cached_index.has_value())
            {
                LOG_DEBUG << "Using cached shard index for " << subdir.name();
                std::string label = "Fetch Shards' Index for " + subdir.name();
                if (label.length() > 85)
                {
                    label = label.substr(0, 82) + "...";
                }
                Console::instance().print(fmt::format("{:<85} {:>20}", label, "✔ Done"));
                return std::optional<ShardsIndexDict>(std::move(cached_index.value()));
            }
        }

        // Print message when fetching shard index
        std::string label = "Fetch Shards' Index for " + subdir.name();
        if (label.length() > 85)
        {
            label = label.substr(0, 82) + "...";
        }
        Console::instance().print(fmt::format("{:<85} {:>20}", label, "⧖ Starting"));

        // Build download request
        auto request_opt = build_shard_index_request(
            subdir,
            params,
            subdir.writable_cache_dir().parent_path()
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
            // Reuse label variable from outer scope
            label = "Fetch Shards' Index for " + subdir.name();
            if (label.length() > 85)
            {
                label = label.substr(0, 82) + "...";
            }
            Console::instance().print(fmt::format("{:<85} {:>20}", label, "⧖ Starting"));
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

            // Print final status when done (reuse label variable)
            if (label.length() > 85)
            {
                label = label.substr(0, 82) + "...";
            }
            Console::instance().print(fmt::format("{:<85} {:>20}", label, "✔ Done"));
            return std::optional<ShardsIndexDict>(std::move(index_result.value()));
        }
        catch (const std::exception& e)
        {
            LOG_DEBUG << "Exception while fetching shard index: " << e.what();
            return std::optional<ShardsIndexDict>{};
        }
    }
}
