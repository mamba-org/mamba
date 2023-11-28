// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_INFO_HPP
#define MAMBA_API_INFO_HPP

#include <string>

namespace mamba
{
    class ChannelContext;
    class Configuration;
    class Context;

    void info(Configuration& config);

    std::string version();

    namespace detail
    {
        void print_info(Context& ctx, ChannelContext& channel_context, const Configuration& config);
    }
}

#endif
