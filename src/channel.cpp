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

    // Stuff initially in url.py but only used in channel.py
    // Might be worth rewriting it based on regex as in original
    // source code
    /*std::pair<std::string, std::string>
    split_platform(const platform_list& known_subdirs, const std::string& url)
    {
        std::string subdir = "";
        size_t pos = std::string::npos;
        for(auto it = known_subdirs.begin(); it != known_subdirs.end(); ++it)
        {
            pos = url.find(*it);
            if (s != std::string::npos)
            {
                subdir = *it;
                break;
            }
        }

        std::string cleaned_url = url;
        if (pos != std::string::npos)
        {
            cleaned_url.replace(pos, subdir.size(), ""); 
        }
        return std::make_pair(cleaned_url.rstrip("/"), subdir);
    }

    Channel get_channel_for_name(const std::string& name)
    {
        std::string stripped, platform;
        std::tie(stripped, platform) = split_platform(Context::instance().known_subdirs, name);
        
        std::string channel = "";
    }*/

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

    const std::string& Channel::subdir() const
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
        if(scheme == "")
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
        // TODO
        return Channel();
    }
    
    Channel Channel::from_value(const std::string& value)
    {
        // TODO
        return Channel();
    }


    /*********************************
     * ChannelContext implementation *
     *********************************/

    ChannelContext::ChannelContext()
        : m_channel_alias(init_channel_alias())
    {
    }

    Channel ChannelContext::init_channel_alias()
    {
        std::string location, scheme, auth, token;
        split_scheme_auth_token(DEFAULT_CHANNEL_ALIAS,
                                location,
                                scheme,
                                auth,
                                token);
        return Channel(scheme, auth, location, token);
    }
}

