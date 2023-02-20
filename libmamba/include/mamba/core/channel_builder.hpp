// Copyright (c) 2021, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CHANNEL_BUILDER_HPP
#define MAMBA_CORE_CHANNEL_BUILDER_HPP

#include "channel.hpp"

namespace mamba
{
    using ChannelCache = std::map<std::string, Channel>;

    class ChannelBuilder
    {
    public:

        static Channel make_simple_channel(
            const Channel& channel_alias,
            const std::string& channel_url,
            const std::string& channel_name = "",
            const std::string& multi_name = ""
        );

        static const Channel& make_cached_channel(const std::string& value);

        static void clear_cache();

    private:

        static ChannelCache& get_cache();

        static Channel from_url(const std::string& url);
        static Channel from_name(const std::string& name);
        static Channel from_value(const std::string& value);
        static Channel from_alias(
            const std::string& scheme,
            const std::string& location,
            const std::optional<std::string>& auth = {},
            const std::optional<std::string>& token = {}
        );

        friend class ChannelContext;
    };

    class ChannelContext
    {
    public:

        using channel_list = std::vector<std::string>;
        using channel_map = std::map<std::string, Channel>;
        using multichannel_map = std::map<std::string, std::vector<std::string>>;

        static ChannelContext& instance();
        void reset();

        ChannelContext(const ChannelContext&) = delete;
        ChannelContext& operator=(const ChannelContext&) = delete;
        ChannelContext(ChannelContext&&) = delete;
        ChannelContext& operator=(ChannelContext&&) = delete;

        // internal
        const Channel& get_channel_alias() const;
        const channel_map& get_custom_channels() const;
        const multichannel_map& get_custom_multichannels() const;

        const channel_list& get_whitelist_channels() const;

    protected:

        ChannelContext();
        ~ChannelContext();

    private:

        Channel build_channel_alias();
        void init_custom_channels();

        Channel m_channel_alias;
        channel_map m_custom_channels;
        multichannel_map m_custom_multichannels;
        channel_list m_whitelist_channels;
    };
}

#endif
