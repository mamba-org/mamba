// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CHANNEL_HPP
#define MAMBA_CORE_CHANNEL_HPP

#include <string>
#include <string_view>

#include "mamba/specs/channel.hpp"

namespace mamba
{
    class Context;

    class ChannelContext
    {
    public:

        using channel_map = specs::ChannelResolveParams::channel_map;
        using channel_list = specs::ChannelResolveParams::channel_list;
        using multichannel_map = specs::ChannelResolveParams::multichannel_map;
        using platform_list = specs::ChannelResolveParams::platform_list;

        ChannelContext(Context& context);

        auto make_channel(std::string_view name) -> channel_list;

        auto get_channel_alias() const -> const specs::CondaURL&;
        auto get_custom_channels() const -> const channel_map&;
        auto get_custom_multichannels() const -> const multichannel_map&;

        [[nodiscard]] auto params() const -> const specs::ChannelResolveParams&;

        auto context() const -> Context&
        {
            return m_context;
        }

    private:

        using ChannelCache = std::unordered_map<std::string, channel_list>;

        Context& m_context;
        ChannelCache m_channel_cache;
        specs::ChannelResolveParams m_channel_params;

        void init_custom_channels();
    };
}
#endif
