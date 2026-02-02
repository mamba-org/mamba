// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARD_INDEX_LOADER_HPP
#define MAMBA_CORE_SHARD_INDEX_LOADER_HPP

#include <optional>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/core/subdir_parameters.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/download/parameters.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/authentication_info.hpp"
#include "mamba/specs/channel.hpp"

namespace mamba
{
    /**
     * Fetch and parse shard index from repodata_shards.msgpack.zst.
     *
     * This class handles downloading the shard index file, caching it,
     * and parsing it into a ShardsIndexDict structure.
     */
    class ShardIndexLoader
    {
    public:

        /**
         * Fetch shard index for a subdir.
         *
         * @param subdir The SubdirIndexLoader to fetch shards for.
         * @param params Download parameters.
         * @param auth_info Authentication information.
         * @param mirrors Mirror map for downloads.
         * @param download_options Download options.
         * @param remote_fetch_params Remote fetch parameters.
         * @param shards_ttl Time-to-live for shard cache check in seconds (0 = use default
         * expiration).
         * @return Parsed shard index, or nullopt if shards not available.
         */
        static auto fetch_and_parse_shard_index(
            const SubdirIndexLoader& subdir,
            const SubdirDownloadParams& params,
            const specs::AuthenticationDataBase& auth_info,
            const download::mirror_map& mirrors,
            const download::Options& download_options,
            const download::RemoteFetchParams& remote_fetch_params,
            std::size_t shards_ttl = 0
        ) -> expected_t<std::optional<ShardsIndexDict>>;

        /**
         * Parse downloaded shard index file.
         *
         * This method is public to allow testing of the parsing logic.
         */
        static auto parse_shard_index(const fs::u8path& file_path) -> expected_t<ShardsIndexDict>;

    private:

        /**
         * Build download request for shard index.
         */
        static auto build_shard_index_request(
            const SubdirIndexLoader& subdir,
            const SubdirDownloadParams& params,
            const fs::u8path& cache_dir
        ) -> std::optional<download::Request>;

        /**
         * Get cache path for shard index.
         */
        static auto shard_index_cache_path(const SubdirIndexLoader& subdir) -> fs::u8path;
    };

}

#endif
