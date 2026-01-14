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
     * Base class for shard-like objects.
     *
     * Provides a unified interface for both sharded repodata (Shards)
     * and traditional repodata presented as shards (ShardLike).
     */
    class ShardBase
    {
    public:

        virtual ~ShardBase() = default;

        /** Return the names of all packages available in this shard collection. */
        [[nodiscard]] virtual auto package_names() const -> std::vector<std::string> = 0;

        /** Check if a package is available in this shard collection. */
        [[nodiscard]] auto contains(const std::string& package) const -> bool;

        /** Return shard URL for a given package. */
        [[nodiscard]] virtual auto shard_url(const std::string& package) const -> std::string = 0;

        /** Return True if the given package's shard is in memory. */
        [[nodiscard]] virtual auto shard_loaded(const std::string& package) const -> bool = 0;

        /** Return a shard that is already loaded in memory. */
        [[nodiscard]] virtual auto visit_package(const std::string& package) const -> ShardDict = 0;

        /** Store new shard data. */
        virtual void visit_shard(const std::string& package, const ShardDict& shard) = 0;

        /** Fetch an individual shard for the given package. */
        virtual auto fetch_shard(const std::string& package) -> expected_t<ShardDict> = 0;

        /** Fetch multiple shards in one go. */
        virtual auto fetch_shards(const std::vector<std::string>& packages)
            -> expected_t<std::map<std::string, ShardDict>>
            = 0;

        /** Return monolithic repodata including all visited shards. */
        [[nodiscard]] virtual auto build_repodata() const -> RepodataDict = 0;

        /** Get the base URL for packages. */
        [[nodiscard]] virtual auto base_url() const -> std::string = 0;

        /** Get the URL of this shard collection. */
        [[nodiscard]] virtual auto url() const -> std::string = 0;
    };

    /**
     * Handle repodata_shards.msgpack.zst and individual per-package shards.
     *
     * This class manages fetching and caching of individual shards from
     * a sharded repodata index.
     */
    class Shards : public ShardBase
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

        [[nodiscard]] auto package_names() const -> std::vector<std::string> override;

        [[nodiscard]] auto shard_url(const std::string& package) const -> std::string override;

        [[nodiscard]] auto shard_loaded(const std::string& package) const -> bool override;

        [[nodiscard]] auto visit_package(const std::string& package) const -> ShardDict override;

        void visit_shard(const std::string& package, const ShardDict& shard) override;

        auto fetch_shard(const std::string& package) -> expected_t<ShardDict> override;

        auto fetch_shards(const std::vector<std::string>& packages)
            -> expected_t<std::map<std::string, ShardDict>> override;

        [[nodiscard]] auto build_repodata() const -> RepodataDict override;

        [[nodiscard]] auto base_url() const -> std::string override;

        [[nodiscard]] auto url() const -> std::string override;

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

    /**
     * Present a "classic" repodata.json as per-package shards.
     *
     * This adapter allows treating monolithic repodata.json as if it were
     * sharded, enabling unified traversal algorithms.
     */
    class ShardLike : public ShardBase
    {
    public:

        /**
         * Create a ShardLike instance from monolithic repodata.
         *
         * @param repodata The monolithic repodata dictionary.
         * @param url URL identifier for this repodata (must be unique).
         */
        ShardLike(RepodataDict repodata, std::string url);

        [[nodiscard]] auto package_names() const -> std::vector<std::string> override;

        [[nodiscard]] auto shard_url(const std::string& package) const -> std::string override;

        [[nodiscard]] auto shard_loaded(const std::string& package) const -> bool override;

        [[nodiscard]] auto visit_package(const std::string& package) const -> ShardDict override;

        void visit_shard(const std::string& package, const ShardDict& shard) override;

        auto fetch_shard(const std::string& package) -> expected_t<ShardDict> override;

        auto fetch_shards(const std::vector<std::string>& packages)
            -> expected_t<std::map<std::string, ShardDict>> override;

        [[nodiscard]] auto build_repodata() const -> RepodataDict override;

        [[nodiscard]] auto base_url() const -> std::string override;

        [[nodiscard]] auto url() const -> std::string override;

    private:

        /** Repodata without packages (info section). */
        RepodataDict m_repodata_no_packages;

        /** Per-package shards split from monolithic repodata. */
        std::map<std::string, ShardDict> m_shards;

        /** Visited shards. */
        std::map<std::string, ShardDict> m_visited;

        /** URL identifier. */
        std::string m_url;
    };

}

#endif
