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

        using ChannelResolveParams = specs::ChannelResolveParams;
        using channel_list = ChannelResolveParams::channel_list;

        [[nodiscard]] static auto make_simple(Context& ctx) -> ChannelContext;

        [[nodiscard]] static auto make_conda_compatible(Context& ctx) -> ChannelContext;

        explicit ChannelContext(Context& ctx);
        ChannelContext(Context& ctx, ChannelResolveParams params);

        auto make_channel(std::string_view name) -> const channel_list&;

        [[nodiscard]] auto params() const -> const specs::ChannelResolveParams&;

        [[nodiscard]] auto context() const -> const Context&;

    private:

        using ChannelCache = std::unordered_map<std::string, channel_list>;

        ChannelResolveParams m_channel_params;
        ChannelCache m_channel_cache;
        const Context& m_context;
    };
}
#endif
