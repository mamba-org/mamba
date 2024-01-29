// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_REMOVE_HPP
#define MAMBA_API_REMOVE_HPP

#include <string>
#include <vector>

#include "mamba/api/constants.hpp"

namespace mamba
{
    class Context;
    class ChannelContext;
    class Configuration;

    void remove(Configuration& config, int flags = MAMBA_REMOVE_PRUNE);

    namespace detail
    {
        void remove_specs(
            Context& ctx,
            ChannelContext& channel_context,
            const std::vector<std::string>& specs,
            bool prune,
            bool force
        );
    }
}

#endif
