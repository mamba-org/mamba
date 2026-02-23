// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_CHANNEL_LOADER_HPP
#define MAMBA_API_CHANNEL_LOADER_HPP

#include <set>
#include <string>
#include <vector>

#include "mamba/core/error_handling.hpp"

namespace mamba
{
    namespace solver::libsolv
    {
        class Database;
        struct Priorities;
        class RepoInfo;
    }
    class Context;
    class SubdirIndexLoader;

    /**
     * Load a single subdir using sharded repodata (only reachable packages).
     *
     * Uses the shard index and per-package shards to load just the packages reachable from
     * \p root_packages via dependencies, instead of the full repodata. Fails fast if shards
     * are disabled, not up-to-date, or \p root_packages is empty.
     *
     * @param ctx Context (repodata_use_shards, shard TTL, download params, etc.).
     * @param database Libsolv database to add repos into.
     * @param root_packages Root package names for reachability (e.g. install specs).
     * @param subdirs All subdir loaders; \p subdir_idx is the one to load.
     * @param subdir_idx Index of the subdir to load in \p subdirs.
     * @param loaded_subdirs_with_shards Set of subdir names already loaded via shards (updated).
     * @param priorities Repo priorities aligned with \p subdirs.
     * @return The repo for the requested subdir, or unexpected mamba_error on failure.
     */
    auto load_subdir_with_shards(
        Context& ctx,
        solver::libsolv::Database& database,
        const std::vector<std::string>& root_packages,
        std::vector<SubdirIndexLoader>& subdirs,
        std::size_t subdir_idx,
        std::set<std::string>& loaded_subdirs_with_shards,
        const std::vector<solver::libsolv::Priorities>& priorities
    ) -> expected_t<solver::libsolv::RepoInfo>;

    class ChannelContext;
    class MultiPackageCache;

    /**
     * Creates channels and mirrors objects and loads channels.
     *
     * Creates and stores channels in the ChannelContext, and mirrors objects in the Context
     * object. Then loads channels, i.e. download repodata.json files if they are not cached
     * locally.
     *
     * @param ctx The context object containing configuration and mirrors.
     * @param channel_context The channel context where channels are created and stored.
     * @param database The libsolv database where channel data is loaded.
     * @param package_caches The package caches used for downloading and caching packages.
     * @param root_packages When non-empty and repodata_use_shards is true, use sharded
     *                      repodata to load only reachable packages from these roots (faster for
     *                      install/update).
     */
    auto load_channels(
        Context& ctx,
        ChannelContext& channel_context,
        solver::libsolv::Database& database,
        MultiPackageCache& package_caches,
        const std::vector<std::string>& root_packages = {}
    ) -> expected_t<void, mamba_aggregated_error>;

    /* Brief Creates channels and mirrors objects,
     * but does not load channels.
     *
     * Creates and stores channels in the ChannelContext,
     * and mirrors objects in the Context object.
     */
    void init_channels(Context& context, ChannelContext& channel_context);
    void init_channels_from_package_urls(
        Context& context,
        ChannelContext& channel_context,
        const std::vector<std::string>& specs
    );
}
#endif
