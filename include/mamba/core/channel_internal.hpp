// Copyright (c) 2021, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

/** Contains the internal interface within Channel, exposed mainly for testing purposes. **/

#ifndef MAMBA_CORE_CHANNEL_INTERNAL_HPP
#define MAMBA_CORE_CHANNEL_INTERNAL_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "channel.hpp"

namespace mamba
{
    class ChannelInternal : public Channel
    {
    public:
        static Channel make_simple_channel(const Channel& channel_alias,
                                           const std::string& channel_url,
                                           const std::string& channel_name = "",
                                           const std::string& multi_name = "");

        static const Channel& make_cached_channel(const std::string& value);

        static void clear_cache();

    private:
        ChannelInternal(const std::string& scheme,
                        const std::string& location,
                        const std::string& name,
                        const std::optional<std::string>& auth = {},
                        const std::optional<std::string>& token = {},
                        const std::optional<std::string>& package_filename = {},
                        const std::optional<std::string>& canonical_name = {});

        using cache_type = std::map<std::string, Channel>;
        static cache_type& get_cache();

        static ChannelInternal from_url(const std::string& url);
        static ChannelInternal from_name(const std::string& name);
        static ChannelInternal from_value(const std::string& value);
        static ChannelInternal from_alias(const std::string& scheme,
                                          const std::string& location,
                                          const std::optional<std::string>& auth = {},
                                          const std::optional<std::string>& token = {});
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

    private:
        ChannelContext();
        ~ChannelContext() = default;

        Channel build_channel_alias();
        void init_custom_channels();

        Channel m_channel_alias;
        channel_map m_custom_channels;
        multichannel_map m_custom_multichannels;
        channel_list m_whitelist_channels;
    };

}  // namespace mamba

#endif
