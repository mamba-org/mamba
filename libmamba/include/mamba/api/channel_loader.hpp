// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_CHANNEL_LOADER_HPP
#define MAMBA_API_CHANNEL_LOADER_HPP

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

    // Creates channels and mirrors objects
    // and loads channels, i.e. download
    // repodata.json files if they are not
    // cached locally.
    auto load_channels(  //
        Context& ctx,
        ChannelContext& channel_context,
        solver::libsolv::Database& pool,
        MultiPackageCache& package_caches
    ) -> expected_t<void, mamba_aggregated_error>;

    // Creates channels and mirrors objects,
    // but does not load channels.
    void init_channels(Context& context, ChannelContext& channel_context);
}
#endif
