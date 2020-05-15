#include <regex>
#include <set>
#include <tuple>
#include <utility>

#include "channel.hpp"
#include "context.hpp"
#include "package_handling.hpp"
#include "url.hpp"
#include "util.hpp"

namespace mamba
{
    // Constants used by Channel and ChannelContext
    namespace
    {
        const std::string DEFAULT_CHANNEL_ALIAS = "https://conda.anaconda.org";
        const std::map<std::string, std::string> DEFAULT_CUSTOM_CHANNELS = {{"pkgs/pro", "https://repo.anaconda.com"}};
        const std::string UNKNOWN_CHANNEL = "<unknown>";
        
        const std::set<std::string> INVALID_CHANNELS =
        {
            "<unknown>",
            "None:///<unknown>",
            "None",
            "",
            ":///<unknown>"
        };

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

        const std::vector<std::string> KNOWN_PLATFORMS =
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
                     const std::string& package_filename,
                     const std::string& multi_name)
        : m_scheme(scheme)
        , m_auth(auth)
        , m_location(location)
        , m_token(token)
        , m_name(name)
        , m_platform(name)
        , m_package_filename(package_filename)
        , m_canonical_name(multi_name)
    {
        if (m_canonical_name == "")
        {
            auto it = ChannelContext::instance().get_custom_channels().find(name);
            if (it != ChannelContext::instance().get_custom_channels().end())
            {
                m_canonical_name = it->first;
            }
            else if (m_scheme != "")
            {
                m_canonical_name = m_scheme + "://" + m_location + '/' + m_name;
            }
            else
            {
                m_canonical_name = lstrip(m_location + '/' + m_name, "/");
            }
        }
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

    const std::string& Channel::package_filename() const
    {
        return m_package_filename;
    }

    const std::string& Channel::canonical_name() const
    {
        return m_canonical_name;
    }

    std::string Channel::url(bool with_credential) const
    {
        std::string base = location();
        if (with_credential && token() != "")
        {
            base += "/t/" + token();
        }
        base += "/" + name();
        if (platform() != "")
        {
            base += "/" + platform();
            if (package_filename() != "")
            {
                base += "/" + package_filename();
            }
        }
        else
        {
            // TODO: handle unknwon archs that are not "noarch"
            base += "/noarch";
        }

        if (with_credential && auth() != "")
        {
            return scheme() + "://" + auth() + "@" + base;
        }
        else
        {
            return scheme() + "://" + base;
        }
    }

    Channel Channel::make_simple_channel(const Channel& channel_alias,
                                         const std::string& channel_url,
                                         const std::string& channel_name,
                                         const std::string& multi_name)
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
                URLHandler parser(location);
                location = parser.url();
                name = parser.path();
            }
        }
        name = name != "" ? strip(name, "/") : strip(channel_url, "/");
        return Channel(scheme, auth, location, token, name, "", "", multi_name);
    }

    Channel& Channel::make_cached_channel(const std::string& value)
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

    void split_conda_url(const std::string& url,
                         std::string& scheme,
                         std::string& host,
                         std::string& port,
                         std::string& path,
                         std::string& auth,
                         std::string& token,
                         std::string& platform,
                         std::string& package_name)
    {
        std::string cleaned_url, extension;
        split_anaconda_token(cleaned_url, cleaned_url, token);
        split_platform(KNOWN_PLATFORMS, cleaned_url, cleaned_url, platform);
        split_package_extension(cleaned_url, cleaned_url, extension);
        
        if (extension != "")
        {
            auto sp = rsplit(cleaned_url, "/", 1);
            cleaned_url = sp[0];
            package_name = sp[1];
        }
        else
        {
            package_name = "";
        }

        URLHandler handler(cleaned_url);
        scheme = handler.scheme();
        host = handler.host();
        port = handler.port();
        path = handler.path();
        auth = handler.auth();
    }

    struct channel_configuration
    {
        channel_configuration(const std::string& location,
                              const std::string& name,
                              const std::string& scheme,
                              const std::string& auth,
                              const std::string& token)
            : m_location(location)
            , m_name(name)
            , m_scheme(scheme)
            , m_auth(auth)
            , m_token(token)
        {
        }

        std::string m_location;
        std::string m_name;
        std::string m_scheme;
        std::string m_auth;
        std::string m_token;
    };

    channel_configuration read_channel_configuration(const std::string& scheme,
                                                     const std::string& host,
                                                     const std::string& port,
                                                     const std::string& path)
    {
        std::string spath = std::string(rstrip(path, "/"));
        std::string url = URLHandler().set_host(host).set_port(port).set_path(spath).url();

        // Case 1: No path given, channel name is ""
        if (spath == "")
        {
            URLHandler handler;
            handler.set_host(host).set_port(port);
            return channel_configuration(std::string(rstrip(handler.url(), "/")), "", scheme, "", "");
        }

        // Case 2: migrated_custom_channels not implemented yet
        // Case 3: migrated_channel_aliases not implemented yet

        // Case 4: custom_channels matches
        const auto& custom_channels = ChannelContext::instance().get_custom_channels();
        for (const auto& ca: custom_channels)
        {
            const Channel& channel = ca.second;
            std::string test_url = channel.location() + '/' + channel.name();
            if (starts_with(url, test_url)) // original code splits with '/' and compares tokens
            {
                auto subname = std::string(strip(url.replace(0u, test_url.size(), ""), "/"));
                return channel_configuration(channel.location(),
                                             channel.name() + '/' + subname,
                                             scheme,
                                             channel.auth(),
                                             channel.token());
            }
        }

        // Case 5: channel_alias match
        const Channel& ca = ChannelContext::instance().get_channel_alias();
        if (ca.location() != "" && starts_with(url, ca.location()))
        {
            auto name = std::string(strip(url.replace(0u, ca.location().size(), ""), "/"));
            return channel_configuration(ca.location(), name, scheme, ca.auth(), ca.token());
        }

        // Case 6: not-otherwise-specified file://-type urls
        if (host == "")
        {
            auto sp = rsplit(url, "/", 1);
            return channel_configuration(sp[0].size() ? sp[0] : "/",
                                         sp[1],
                                         "file",
                                         "",
                                         "");
        }

        // Case 7: fallback, channel_location = host:port and channel_name = path,
        //         bumps the first token of paths starting with /conda for compatibiliy
        //         with Anaconda Enterprise Repository software
        spath = lstrip(spath, "/");
        std::string bump = "";
        if (starts_with(spath, "conda"))
        {
            bump = "conda";
            spath = spath.replace(0, 6, "");
        }
        std::string location = URLHandler().set_host(host).set_port(port).set_path(bump).url();
        return channel_configuration(std::string(strip(location, "/")),
                                     spath,
                                     scheme,
                                     "",
                                     "");
    }

    Channel Channel::from_url(const std::string& url)
    {
        std::string scheme, host, port, path, auth, token, platform, package_name;
        split_conda_url(url, scheme, host, port, path, auth, token, platform, package_name);

        auto config = read_channel_configuration(scheme,
                                                 host,
                                                 port,
                                                 path);

        return Channel(config.m_scheme.size() ? config.m_scheme : "https",
                       auth.size() ? auth : config.m_auth,
                       config.m_location,
                       token.size() ? token : config.m_token,
                       config.m_name,
                       platform,
                       package_name);
    }
    
    Channel Channel::from_name(const std::string& name)
    {
        std::string stripped, platform;
        split_platform(KNOWN_PLATFORMS, name, stripped, platform);

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
    
    std::string fix_win_path(const std::string& path)
    {
#ifdef _WIN32
        if (starts_with(path, "file:"))
        {
            std::regex re(R"(\\(?! )");
            std::string res = std::regex_replace(path, re, R"(/)");
            replace_all(res, ":////", "://");
            return res;
        }
        else
        {
            return path;
        }
#else
        return path;
#endif
    }

    Channel Channel::from_value(const std::string& value)
    {
        if (INVALID_CHANNELS.find(value) != INVALID_CHANNELS.end())
        {
            return Channel("", "", "", "", UNKNOWN_CHANNEL);
        }

        if (has_scheme(value))
        {
            return Channel::from_url(fix_win_path(value));
        }

        if (is_path(value))
        {
            return Channel::from_url(path_to_url(value));
        }

        if (is_package_file(value))
        {
            return Channel::from_url(fix_win_path(value));
        }

        return Channel::from_name(value);
    }

    Channel& make_channel(const std::string& value)
    {
        return Channel::make_cached_channel(value);
    }

    /*********************************
     * ChannelContext implementation *
     *********************************/

    ChannelContext& ChannelContext::instance()
    {
        static ChannelContext context;
        return context;
    }

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
            auto channel = Channel::make_simple_channel(m_channel_alias, url, "defaults");
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

