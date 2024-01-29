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
        using Channel = specs::Channel;
        using channel_list = ChannelResolveParams::channel_list;

        /**
         * Create a ChannelContext with a simple parsing of the context options.
         *
         * No hardcoded names are added.
         * Custom channels are treated as aliases rather than the Conda way (the name is not
         * added at the end of the URL if absent).
         */
        [[nodiscard]] static auto make_simple(const Context& ctx) -> ChannelContext;

        /**
         * Create a ChannelContext while applying all of Conda context options.
         *
         * If not defined, the Conda custom channels "pkgs/main", "pkgs/r", "pkgs/pro",
         * and "pkgs/msys2" (Windows only) will be added.
         * If not defined, the Conda custom mutlit channels "defaults" and "local" will
         * be added.
         * The function will ensure custom channels names are added at the end of the URLs.
         */
        [[nodiscard]] static auto make_conda_compatible(const Context& ctx) -> ChannelContext;

        /**
         * Initialize channel with the parameters as they are.
         *
         * The Context is not parsed.
         */
        ChannelContext(ChannelResolveParams params, std::vector<Channel> has_zst);

        [[nodiscard]] auto make_channel(specs::UnresolvedChannel uc) -> const channel_list&;
        [[nodiscard]] auto make_channel(std::string_view name) -> const channel_list&;
        [[nodiscard]] auto
        make_channel(std::string_view name, const std::vector<std::string>& mirrors)
            -> const channel_list&;

        [[nodiscard]] auto params() const -> const specs::ChannelResolveParams&;

        [[nodiscard]] auto has_zst(const Channel& chan) const -> bool;

    private:

        using ChannelCache = std::unordered_map<std::string, channel_list>;

        ChannelResolveParams m_channel_params;
        ChannelCache m_channel_cache;
        std::vector<Channel> m_has_zst;
    };
}
#endif
