#ifndef MAMBA_CHANNEL_HPP
#define MAMBA_CHANNEL_HPP

#include <map>
#include <string>

namespace mamba
{

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

        const std::string& scheme() const;
        const std::string& auth() const;
        const std::string& location() const;
        const std::string& token() const;
        const std::string& name() const;
        const std::string& platform() const;
        const std::string& package_filename() const;
        const std::string& canonical_name() const;

        std::string url(bool with_credential = true) const;
        
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

    class ChannelContext
    {
    public:

        using channel_map = std::map<std::string, Channel>;

        static ChannelContext& instance();

        ChannelContext(const ChannelContext&) = delete;
        ChannelContext& operator=(const ChannelContext&) = delete;
        ChannelContext(ChannelContext&&) = delete;
        ChannelContext& operator=(ChannelContext&&) = delete;

        const Channel& get_channel_alias() const;
        const channel_map& get_custom_channels() const;

    private:

        ChannelContext();
        ~ChannelContext() = default;

        Channel build_channel_alias();
        channel_map build_custom_channels();

        Channel m_channel_alias;
        channel_map m_custom_channels;
    };

}

#endif
