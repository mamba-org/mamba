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
    class Context;
    class MPool;
    class MultiPackageCache;

    expected_t<void, mamba_aggregated_error>
    load_channels(Context& ctx, MPool& pool, MultiPackageCache& package_caches);
}
#endif
