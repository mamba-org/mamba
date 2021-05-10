// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CHANNEL_HPP
#define MAMBA_CORE_CHANNEL_HPP

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace mamba
{
    void load_tokens();

    class Channel
    {
    public:
        Channel(const std::string& scheme = "",
                const std::string& auth = "",
                const std::string& location = "",
                const std::string& token = "",
                const std::string& name = "",
                const std::string& platform = "",
                const std::string& package_filename = "",
                const std::string& multi_name = "");

        void set_token(const std::string& token);
        const std::string& scheme() const;
        const std::string& auth() const;
        const std::string& location() const;
        const std::string& token() const;
        const std::string& name() const;
        const std::string& platform() const;
        const std::string& package_filename() const;
        const std::string& canonical_name() const;

        std::string base_url() const;
        std::string url(bool with_credential = true) const;

        std::vector<std::string> urls(bool with_credential = true) const;
        std::vector<std::string> urls(const std::vector<std::string>& platforms,
                                      bool with_credential = true) const;

        static Channel make_simple_channel(const Channel& channel_alias,
                                           const std::string& channel_url,
                                           const std::string& channel_name = "",
                                           const std::string& multi_name = "");

        static Channel& make_cached_channel(const std::string& value);
        static void clear_cache();

    private:
        using cache_type = std::map<std::string, Channel>;
        static cache_type& get_cache();

        static Channel from_url(const std::string& url);
        static Channel from_name(const std::string& name);
        static Channel from_value(const std::string& value);

        std::string build_url(const std::string& base, bool with_credential) const;

        std::string m_scheme;
        std::string m_auth;
        std::string m_location;
        std::string m_token;
        std::string m_name;
        std::string m_platform;
        std::string m_package_filename;
        mutable std::string m_canonical_name;
    };

    Channel& make_channel(const std::string& value);

    std::vector<std::string> get_channel_urls(const std::vector<std::string>& channel_names,
                                              const std::vector<std::string>& platforms = {},
                                              bool with_credential = true);

    std::vector<std::string> calculate_channel_urls(const std::vector<std::string>& channel_names,
                                                    bool append_context_channels = true,
                                                    const std::string& platform = "",
                                                    bool use_local = false);

    void check_whitelist(const std::vector<std::string>& urls);

    class ChannelContext
    {
    public:
        using channel_list = std::vector<std::string>;
        using channel_map = std::map<std::string, Channel, std::less<>>;
        using multichannel_map = std::map<std::string, std::vector<std::string>>;

        static ChannelContext& instance();

        ChannelContext(const ChannelContext&) = delete;
        ChannelContext& operator=(const ChannelContext&) = delete;
        ChannelContext(ChannelContext&&) = delete;
        ChannelContext& operator=(ChannelContext&&) = delete;

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
