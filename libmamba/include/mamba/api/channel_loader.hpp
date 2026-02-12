// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_CHANNEL_LOADER_HPP
#define MAMBA_API_CHANNEL_LOADER_HPP

#include <string>
#include <vector>

#include "mamba/core/error_handling.hpp"

namespace mamba
{
    namespace solver::libsolv
    {
        class Database;
    }
    class Context;
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
