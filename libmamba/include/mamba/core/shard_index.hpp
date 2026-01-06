// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARD_INDEX_HPP
#define MAMBA_CORE_SHARD_INDEX_HPP

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
         * @return Parsed shard index, or nullopt if shards not available.
         */
        static auto fetch_shards_index(
            const SubdirIndexLoader& subdir,
            const SubdirDownloadParams& params,
            const specs::AuthenticationDataBase& auth_info,
            const download::mirror_map& mirrors,
            const download::Options& download_options,
            const download::RemoteFetchParams& remote_fetch_params
        ) -> expected_t<std::optional<ShardsIndexDict>>;

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
         * Parse downloaded shard index file.
         */
        static auto parse_shard_index(const fs::u8path& file_path) -> expected_t<ShardsIndexDict>;

        /**
         * Get cache path for shard index.
         */
        static auto shard_index_cache_path(const SubdirIndexLoader& subdir) -> fs::u8path;
    };

}

#endif
