// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SHARD_LOADER_HPP
#define MAMBA_CORE_SHARD_LOADER_HPP

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
         * @param mirrors Mirror map for downloads.
         * @param remote_fetch_params Remote fetch parameters.
         * @param download_threads Number of threads to use for parallel shard fetching.
         */
        Shards(
            ShardsIndexDict shards_index,
            std::string url,
            const specs::Channel& channel,
            const specs::AuthenticationDataBase& auth_info,
            const download::mirror_map& mirrors,
            const download::RemoteFetchParams& remote_fetch_params,
            std::size_t download_threads = 10
        );

        /** Return the names of all packages available in this shard collection. */
        [[nodiscard]] auto package_names() const -> std::vector<std::string>;

        /** Check if a package is available in this shard collection. */
        [[nodiscard]] auto contains(const std::string& package) const -> bool;

        /** Return shard URL for a given package. */
        [[nodiscard]] auto shard_url(const std::string& package) const -> std::string;

        /** Return True if the given package's shard is in memory. */
        [[nodiscard]] auto shard_loaded(const std::string& package) const -> bool;

        /** Return a shard that is already loaded in memory. */
        [[nodiscard]] auto visit_package(const std::string& package) const -> ShardDict;

        /** Store new shard data. */
        void visit_shard(const std::string& package, const ShardDict& shard);

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

    private:

        /** Shard index data. */
        ShardsIndexDict m_shards_index;

        /** URL of the shard index file. */
        std::string m_url;

        /** Channel information. */
        specs::Channel m_channel;

        /** Authentication information. */
        specs::AuthenticationDataBase m_auth_info;

        /** Mirror map (reference, not copyable). */
        [[maybe_unused]] const download::mirror_map& m_mirrors;

        /** Remote fetch parameters. */
        download::RemoteFetchParams m_remote_fetch_params;

        /** Number of threads to use for parallel shard fetching. */
        std::size_t m_download_threads;

        /** Visited shards, keyed by package name. */
        std::map<std::string, ShardDict> m_visited;

        /** Cached shards_base_url. */
        mutable std::optional<std::string> m_shards_base_url;

        /**
         * Get the base URL where shards are stored.
         */
        [[nodiscard]] auto shards_base_url() const -> std::string;

        /**
         * Get the relative path for a shard (for use with download::Request).
         * Returns path relative to channel base.
         */
        [[nodiscard]] auto shard_path(const std::string& package) const -> std::string;

        /**
         * Process a fetched shard and add it to visited shards.
         */
        void
        process_fetched_shard(const std::string& url, const std::string& package, const ShardDict& shard);
    };

}

#endif
