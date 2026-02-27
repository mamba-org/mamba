// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARDS_HPP
#define MAMBA_CORE_SHARDS_HPP

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/shard_types.hpp"
#include "mamba/core/subdir_index.hpp"
#include "mamba/download/downloader.hpp"
#include "mamba/download/parameters.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/authentication_info.hpp"
#include "mamba/specs/channel.hpp"

namespace mamba
{
    // Forward declaration
    class TemporaryFile;

    /**
     * Handle repodata_shards.msgpack.zst and individual per-package shards.
     *
     * This class manages fetching and caching of individual shards from
     * a sharded repodata index.
     */
    class Shards
    {
    public:

        /**
         * Create a Shards instance from a shard index.
         *
         * @param shards_index The parsed shard index.
         * @param url URL of repodata_shards.msgpack.zst.
         * @param channel Channel information for downloads.
         * @param auth_info Authentication information.
         * @param remote_fetch_params Remote fetch parameters.
         * @param download_threads Number of threads to use for parallel shard fetching.
         * @param mirrors Optional base mirrors for channel-based downloads. When provided,
         *        extend_mirrors in fetch_shards will be initialized from these before adding
         *        absolute-URL mirrors.
         */
        Shards(
            ShardsIndexDict shards_index,
            std::string url,
            specs::Channel channel,
            specs::AuthenticationDataBase auth_info,
            download::RemoteFetchParams remote_fetch_params,
            std::size_t download_threads = 10,
            std::optional<std::reference_wrapper<const download::mirror_map>> mirrors = std::nullopt
        );

        /** Return the names of all packages available in this shard collection. */
        [[nodiscard]] auto package_names() const -> std::vector<std::string>;

        /** Check if a package is available in this shard collection. */
        [[nodiscard]] auto contains(const std::string& package) const -> bool;

        /** Return shard URL for a given package. */
        [[nodiscard]] auto shard_url(const std::string& package) const -> std::string;

        /** Return True if the given package's shard is in memory. */
        [[nodiscard]] auto is_shard_present(const std::string& package) const -> bool;

        /** Return a shard that is already loaded in memory. */
        [[nodiscard]] auto visit_package(const std::string& package) const -> ShardDict;

        /** Process a fetched shard and add it to visited shards. */
        void process_fetched_shard(const std::string& package, const ShardDict& shard);

        /** Fetch an individual shard for the given package. */
        auto fetch_shard(const std::string& package) -> expected_t<ShardDict>;

        /** Fetch multiple shards in one go. */
        auto fetch_shards(const std::vector<std::string>& packages)
            -> expected_t<std::map<std::string, ShardDict>>;

        /** Return monolithic repodata including all visited shards. */
        [[nodiscard]] auto build_repodata() const -> RepodataDict;

        /** Get the base URL for packages. */
        [[nodiscard]] auto base_url() const -> std::string;

        /** Get the URL of this shard collection. */
        [[nodiscard]] auto url() const -> std::string;

        /** Get the subdir (platform) from the shard index. */
        [[nodiscard]] auto subdir() const -> const std::string&;

    private:

        /** Shard index data. */
        ShardsIndexDict m_shards_index;

        /** URL of the shard index file. */
        std::string m_url;

        /** Channel information. */
        specs::Channel m_channel;

        /** Authentication information. */
        specs::AuthenticationDataBase m_auth_info;

        /** Remote fetch parameters. */
        download::RemoteFetchParams m_remote_fetch_params;

        /** Number of threads to use for parallel shard fetching. */
        std::size_t m_download_threads;

        /** Optional base mirrors for channel-based downloads. */
        std::optional<std::reference_wrapper<const download::mirror_map>> m_mirrors;

        /** Visited shards, keyed by package name. */
        std::map<std::string, ShardDict> m_visited;

        /** Cached shards_base_url. */
        mutable std::optional<std::string> m_shards_base_url;

        /** Cached resolved base_url for packages. */
        mutable std::optional<std::string> m_base_url_cache;

        /**
         * Get the base URL where shards are stored.
         */
        [[nodiscard]] auto shards_base_url() const -> std::string;

        /**
         * Get the relative path for a shard (for use with download::Request).
         * Returns path relative to channel base.
         */
        [[nodiscard]] auto relative_shard_path(const std::string& package) const -> std::string;

        /**
         * Filter packages into those that need fetching vs already in memory.
         */
        void filter_packages_to_fetch(
            const std::vector<std::string>& packages,
            std::map<std::string, ShardDict>& results,
            std::vector<std::string>& packages_to_fetch
        ) const;

        /**
         * Build URLs for packages to fetch and create url_to_package mapping.
         */
        void build_shard_urls(
            const std::vector<std::string>& packages_to_fetch,
            std::vector<std::string>& urls,
            std::map<std::string, std::string>& url_to_package
        ) const;

        /**
         * Create download requests for shards with proper mirror handling.
         */
        void create_download_requests(
            const std::map<std::string, std::string>& url_to_package,
            const std::string& cache_dir_str,
            download::mirror_map& extended_mirrors,
            download::MultiRequest& requests,
            std::vector<std::string>& cache_miss_urls,
            std::vector<std::string>& cache_miss_packages,
            std::map<std::string, fs::u8path>& package_to_artifact_path,
            std::vector<std::shared_ptr<TemporaryFile>>& artifacts
        ) const;

        /**
         * Process a single downloaded shard: handle content types, read, decompress, and parse.
         */
        auto process_downloaded_shard(
            const std::string& package,
            const download::Success& success,
            const std::map<std::string, fs::u8path>& package_to_artifact_path
        ) -> expected_t<ShardDict>;

        /**
         * Decompress zstd compressed shard data.
         */
        auto decompress_zstd_shard(const std::vector<std::uint8_t>& compressed_data)
            -> expected_t<std::vector<std::uint8_t>>;

        /**
         * Parse msgpack data into ShardDict.
         */
        auto
        parse_shard_msgpack(const std::vector<std::uint8_t>& decompressed_data, const std::string& package)
            -> expected_t<ShardDict>;
    };

}

#endif
