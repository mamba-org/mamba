#include <tuple>
#include <utility>

#include "channel.hpp"
#include "context.hpp"
#include "url.hpp"
#include "util.hpp"

namespace mamba
{
    // Constants used by Channel and ChannelContext
    namespace
    {
        const std::string DEFAULT_CHANNEL_ALIAS = "https://conda.anaconda.org";
        const std::map<std::string, std::string> DEFAULT_CUSTOM_CHANNELS = {{"pkgs/pro", "https://repo.anaconda.com"}};

        const std::vector<std::string> DEFAULT_CHANNELS = 
        {
#ifdef _WIN32
            "https://repo.anaconda.com/pkgs/main",
            "https://repo.anaconda.com/pkgs/r",
            "https://repo.anaconda.com/pkgs/msys2"
#else
            "https://repo.anaconda.com/pkgs/main",
            "https://repo.anaconda.com/pkgs/r"
#endif
        };

        const std::vector<std::string> KNOWN_SUBDIRS =
        {
            "noarch",
            "linux-32",
            "linux-64",
            "linux-aarch64",
            "linux-armv6l",
            "linux-armv7l",
            "linux-ppc64",
            "linux-ppc64le",
            "osx-64",
            "win-32",
            "win-64",
            "zos-z"
        };
    }

    /**************************
     * Channel implementation *
     **************************/

    Channel::Channel(const std::string& scheme,
                     const std::string& auth,
                     const std::string& location,
                     const std::string& token,
                     const std::string& name,
                     const std::string& platform,
                     const std::string& package_filename)
        : m_scheme(scheme)
        , m_auth(auth)
        , m_location(location)
        , m_token(token)
        , m_name(name)
        , m_platform(name)
        , m_package_filename(package_filename)
    {
    }

    const std::string& Channel::scheme() const
    {
        return m_scheme;
    }

    const std::string& Channel::auth() const
    {
        return m_auth;
    }

    const std::string& Channel::location() const
    {
        return m_location;
    }

    const std::string& Channel::token() const
    {
        return m_token;
    }

    const std::string& Channel::name() const
    {
        return m_name;
    }

    const std::string& Channel::platform() const
    {
        return m_platform;
    }

    Channel Channel::make_simple_channel(const Channel& channel_alias,
                                         const std::string& channel_url,
                                         const std::string& channel_name)
    {
        std::string name(channel_name);
        std::string location, scheme, auth, token;
        split_scheme_auth_token(channel_url,
                                location,
                                scheme,
                                auth,
                                token);
        if (scheme == "")
        {
            location = channel_alias.location();
            scheme = channel_alias.scheme();
            auth = channel_alias.auth();
            token = channel_alias.token();
        }
        else if(name == "")
        {
            if (channel_alias.location() != "" && starts_with(location, channel_alias.location()))
            {
                name = location;
                name.replace(0u, channel_alias.location().size(), "");
                location = channel_alias.location();
            }
            else
            {
                URLParser parser(location);
                location = parser.url();
                name = parser.path();
            }
        }
        name = name != "" ? strip(name, "/") : strip(channel_url, "/");
        return Channel(scheme, auth, location, token, name);
    }

    const Channel& Channel::make_cached_channel(const std::string& value)
    {
        auto res = get_cache().find(value);
        if (res == get_cache().end())
        {
            res = get_cache().insert(std::make_pair(value, Channel::from_value(value))).first;
        }
        return res->second;
    }

    void Channel::clear_cache()
    {
        get_cache().clear();
    }

    Channel::cache_type& Channel::get_cache()
    {
        static cache_type cache;
        return cache;
    }

    Channel Channel::from_url(const std::string& url)
    {
        // TODO
        return Channel();
    }
    
    Channel Channel::from_name(const std::string& name)
    {
        std::string stripped, platform;
        split_platform(KNOWN_SUBDIRS, name, stripped, platform);

        std::string tmp_stripped = stripped;
        const auto& custom_channels = ChannelContext::instance().get_custom_channels();
        auto it_end = custom_channels.end();
        auto it = custom_channels.find(tmp_stripped);
        while (it == it_end)
        {
            size_t pos = tmp_stripped.rfind("/");
            if (pos == std::string::npos)
            {
                break;
            }
            else
            {
                tmp_stripped = tmp_stripped.substr(0, pos);
                it = custom_channels.find(tmp_stripped);
            }
        }

        if (it != it_end)
        {
            return Channel(it->second.scheme(),
                           it->second.auth(),
                           it->second.location(),
                           it->second.token(),
                           stripped,
                           platform != "" ? platform : it->second.platform(),
                           it->second.package_filename());
        }
        else
        {
            const Channel& alias = ChannelContext::instance().get_channel_alias();
            return Channel(alias.scheme(),
                           alias.auth(),
                           alias.location(),
                           alias.token(),
                           stripped,
                           platform);
        }
    }
    
    Channel Channel::from_value(const std::string& value)
    {
        // TODO
        return Channel();
    }

    /*********************************
     * ChannelContext implementation *
     *********************************/

    const Channel& ChannelContext::get_channel_alias() const
    {
        return m_channel_alias;
    }

    auto ChannelContext::get_custom_channels() const -> const channel_map&
    {
        return m_custom_channels;
    }

    ChannelContext::ChannelContext()
        : m_channel_alias(build_channel_alias())
        , m_custom_channels(build_custom_channels())
    {
    }

    Channel ChannelContext::build_channel_alias()
    {
        std::string location, scheme, auth, token;
        split_scheme_auth_token(DEFAULT_CHANNEL_ALIAS,
                                location,
                                scheme,
                                auth,
                                token);
        return Channel(scheme, auth, location, token);
    }

    ChannelContext::channel_map ChannelContext::build_custom_channels()
    {
        channel_map m;

        for(auto& url: DEFAULT_CHANNELS)
        {
            auto channel = Channel::make_simple_channel(m_channel_alias, url);
            m.emplace(channel.name(), std::move(channel));
        }

        // TODO: add channels based on local build folders

        for(auto& ch: DEFAULT_CUSTOM_CHANNELS)
        {
            m.emplace(ch.first, Channel::make_simple_channel(m_channel_alias, ch.first, ch.second));
        }
        return m;
    }
}

