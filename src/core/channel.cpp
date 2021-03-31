// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    // Constants used by Channel and ChannelContext
    namespace
    {
        const std::map<std::string, std::string> DEFAULT_CUSTOM_CHANNELS
            = { { "pkgs/pro", "https://repo.anaconda.com" } };
        const char UNKNOWN_CHANNEL[] = "<unknown>";

        const std::set<std::string> INVALID_CHANNELS
            = { "<unknown>", "None:///<unknown>", "None", "", ":///<unknown>" };

        const char LOCAL_CHANNELS_NAME[] = "local";
        const char DEFAULT_CHANNELS_NAME[] = "defaults";

        // ATTENTION names with substrings need to go longer -> smalle
        // otherwise linux-ppc64 matches for linux-ppc64le etc!
        const std::vector<std::string> KNOWN_PLATFORMS
            = { "noarch",       "linux-32",      "linux-64",    "linux-aarch64", "linux-armv6l",
                "linux-armv7l", "linux-ppc64le", "linux-ppc64", "osx-64",        "osx-arm64",
                "win-32",       "win-64",        "zos-z" };
    }  // namespace

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
        , m_platform(platform)
        , m_package_filename(package_filename)
        , m_canonical_name(multi_name)
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

    const std::string& Channel::package_filename() const
    {
        return m_package_filename;
    }

    const std::string& Channel::canonical_name() const
    {
        if (m_canonical_name == "")
        {
            auto it = ChannelContext::instance().get_custom_channels().find(m_name);
            if (it != ChannelContext::instance().get_custom_channels().end())
            {
                m_canonical_name = it->first;
            }
            else if (m_location == ChannelContext::instance().get_channel_alias().location())
            {
                m_canonical_name = m_name;
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
        return m_canonical_name;
    }

    std::string Channel::base_url() const
    {
        if (canonical_name() == UNKNOWN_CHANNEL)
        {
            return "";
        }
        else
        {
            return scheme() + "://" + join_url(location(), name());
        }
    }

    std::string Channel::build_url(const std::string& base, bool with_credential) const
    {
        if (with_credential && auth() != "")
        {
            return scheme() + "://" + auth() + "@" + base;
        }
        else
        {
            return scheme() + "://" + base;
        }
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

        return build_url(base, with_credential);
    }

    std::vector<std::string> Channel::urls(bool with_credential) const
    {
        return urls(Context::instance().platforms(), with_credential);
    }

    std::vector<std::string> Channel::urls(const std::vector<std::string>& platforms,
                                           bool with_credential) const
    {
        if (canonical_name() == UNKNOWN_CHANNEL)
            return make_channel(DEFAULT_CHANNELS_NAME).urls(platforms, with_credential);

        std::string base = with_credential && token() != ""
                               ? join_url(location(), "t", token(), name())
                               : join_url(location(), name());

        size_t size = platform() != "" ? (platform() != "noarch" ? 2u : 1u) : platforms.size();
        std::vector<std::string> res(size);
        if (platform() != "")
        {
            res[0] = build_url(join_url(base, platform()), with_credential);
            if (size > 1u)
            {
                res[1] = build_url(join_url(base, "noarch"), with_credential);
            }
        }
        else
        {
            std::transform(platforms.cbegin(),
                           platforms.cend(),
                           res.begin(),
                           [this, &base, with_credential](const std::string& p) {
                               return this->build_url(join_url(base, p), with_credential);
                           });
        }

        return res;
    }

    Channel Channel::make_simple_channel(const Channel& channel_alias,
                                         const std::string& channel_url,
                                         const std::string& channel_name,
                                         const std::string& multi_name)
    {
        std::string name(channel_name);
        std::string location, scheme, auth, token;
        split_scheme_auth_token(channel_url, location, scheme, auth, token);
        if (scheme == "")
        {
            location = channel_alias.location();
            scheme = channel_alias.scheme();
            auth = channel_alias.auth();
            token = channel_alias.token();
        }
        else if (name == "")
        {
            if (channel_alias.location() != "" && starts_with(location, channel_alias.location()))
            {
                name = location;
                name.replace(0u, channel_alias.location().size(), "");
                location = channel_alias.location();
            }
            else
            {
                std::string full_url = scheme + "://" + location;
                URLHandler parser(full_url);
                location = rstrip(
                    URLHandler().set_host(parser.host()).set_port(parser.port()).url(), "/");
                name = lstrip(parser.path(), "/");
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
        split_anaconda_token(url, cleaned_url, token);
        split_platform(KNOWN_PLATFORMS, cleaned_url, cleaned_url, platform);
        split_package_extension(cleaned_url, cleaned_url, extension);

        if (extension != "")
        {
            auto sp = rsplit(cleaned_url, "/", 1);
            cleaned_url = sp[0];
            package_name = sp[1] + extension;
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
        std::string url
            = URLHandler().set_scheme(scheme).set_host(host).set_port(port).set_path(spath).url(
                true);

        // Case 1: No path given, channel name is ""
        if (spath == "")
        {
            URLHandler handler;
            handler.set_host(host).set_port(port);
            return channel_configuration(
                std::string(rstrip(handler.url(), "/")), "", scheme, "", "");
        }

        // Case 2: migrated_custom_channels not implemented yet
        // Case 3: migrated_channel_aliases not implemented yet

        // Case 4: custom_channels matches
        const auto& custom_channels = ChannelContext::instance().get_custom_channels();
        for (const auto& ca : custom_channels)
        {
            const Channel& channel = ca.second;
            std::string test_url = join_url(channel.location(), channel.name());

            // original code splits with '/' and compares tokens
            if (starts_with(url, test_url))
            {
                auto subname = std::string(strip(url.replace(0u, test_url.size(), ""), "/"));
                return channel_configuration(channel.location(),
                                             join_url(channel.name(), subname),
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
            return channel_configuration(sp[0].size() ? sp[0] : "/", sp[1], "file", "", "");
        }

        // Case 7: fallback, channel_location = host:port and channel_name = path
        spath = lstrip(spath, "/");
        std::string location = URLHandler().set_host(host).set_port(port).url();
        return channel_configuration(std::string(strip(location, "/")), spath, scheme, "", "");
    }

    Channel Channel::from_url(const std::string& url)
    {
        std::string scheme, host, port, path, auth, token, platform, package_name;
        split_conda_url(url, scheme, host, port, path, auth, token, platform, package_name);

        auto config = read_channel_configuration(scheme, host, port, path);

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
            return Channel(
                alias.scheme(), alias.auth(), alias.location(), alias.token(), stripped, platform);
        }
    }

    std::string fix_win_path(const std::string& path)
    {
#ifdef _WIN32
        if (starts_with(path, "file:"))
        {
            std::regex re(R"(\\(?! ))");
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

    /************************************
     * utility functions implementation *
     ************************************/

    Channel& make_channel(const std::string& value)
    {
        return Channel::make_cached_channel(value);
    }

    void append_channel_urls(const std::string name,
                             const std::vector<std::string>& platforms,
                             bool with_credential,
                             std::vector<std::string>& result,
                             std::set<std::string>& control)
    {
        bool ret = !control.insert(name).second;
        if (ret)
            return;

        std::vector<std::string> urls = make_channel(name).urls(platforms, with_credential);
        std::copy(urls.begin(), urls.end(), std::back_inserter(result));
    }

    std::vector<std::string> get_channel_urls(const std::vector<std::string>& channel_names,
                                              const std::vector<std::string>& platforms,
                                              bool with_credential)
    {
        std::set<std::string> control;
        std::vector<std::string> result;
        result.reserve(channel_names.size() * platforms.size());
        for (const auto& name : channel_names)
        {
            auto multi_iter = ChannelContext::instance().get_custom_multichannels().find(name);
            if (multi_iter != ChannelContext::instance().get_custom_multichannels().end())
            {
                for (const auto& n : multi_iter->second)
                {
                    append_channel_urls(n, platforms, with_credential, result, control);
                }
            }
            else
            {
                append_channel_urls(name, platforms, with_credential, result, control);
            }
        }
        return result;
    }

    std::vector<std::string> calculate_channel_urls(const std::vector<std::string>& channel_names,
                                                    bool append_context_channels,
                                                    const std::string& platform,
                                                    bool use_local)
    {
        // TODO that doesn't seem very logical
        std::vector<std::string> platforms = platform.size()
                                                 ? std::vector<std::string>({ platform, "noarch" })
                                                 : Context::instance().platforms();

        if (append_context_channels || use_local)
        {
            const auto& ctx_channels = Context::instance().channels;
            std::vector<std::string> names;
            names.reserve(channel_names.size() + 1 + ctx_channels.size());
            if (use_local)
            {
                names.push_back(LOCAL_CHANNELS_NAME);
            }
            std::copy(channel_names.begin(), channel_names.end(), std::back_inserter(names));
            if (append_context_channels)
            {
                std::copy(ctx_channels.begin(), ctx_channels.end(), std::back_inserter(names));
            }
            return get_channel_urls(names, platforms);
        }
        else
        {
            return get_channel_urls(channel_names, platforms);
        }
    }

    void check_whitelist(const std::vector<std::string>& urls)
    {
        const auto& whitelist = ChannelContext::instance().get_whitelist_channels();
        if (whitelist.size())
        {
            std::vector<std::string> accepted_urls(whitelist.size());
            std::transform(whitelist.begin(),
                           whitelist.end(),
                           accepted_urls.begin(),
                           [](const std::string& url) { return make_channel(url).base_url(); });
            std::for_each(urls.begin(), urls.end(), [&accepted_urls](const std::string& s) {
                auto it = std::find(
                    accepted_urls.begin(), accepted_urls.end(), make_channel(s).base_url());
                if (it == accepted_urls.end())
                {
                    std::ostringstream str;
                    str << "Channel " << s << " not allowed";
                    throw std::runtime_error(str.str().c_str());
                }
            });
        }
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

    auto ChannelContext::get_custom_multichannels() const -> const multichannel_map&
    {
        return m_custom_multichannels;
    }

    auto ChannelContext::get_whitelist_channels() const -> const channel_list&
    {
        return m_whitelist_channels;
    }

    ChannelContext::ChannelContext()
        : m_channel_alias(build_channel_alias())
        , m_custom_channels()
        , m_custom_multichannels()
        , m_whitelist_channels()
    {
        init_custom_channels();
    }

    Channel ChannelContext::build_channel_alias()
    {
        auto& ctx = Context::instance();
        std::string alias = ctx.channel_alias;
        std::string location, scheme, auth, token;
        split_scheme_auth_token(alias, location, scheme, auth, token);
        return Channel(scheme, auth, location, token);
    }

    void ChannelContext::init_custom_channels()
    {
        /******************
         * MULTI CHANNELS *
         ******************/

        // Default channels
        auto& default_channels = Context::instance().default_channels;
        std::vector<std::string> default_names(default_channels.size());
        auto name_iter = default_names.begin();
        for (auto& url : default_channels)
        {
            auto channel
                = Channel::make_simple_channel(m_channel_alias, url, "", DEFAULT_CHANNELS_NAME);
            std::string name = channel.name();
            auto res = m_custom_channels.emplace(std::move(name), std::move(channel));
            *name_iter++ = res.first->first;
        }
        m_custom_multichannels.emplace(DEFAULT_CHANNELS_NAME, std::move(default_names));

        // Local channels
        std::vector<std::string> local_channels
            = { Context::instance().target_prefix.string() + "/conda-bld",
                Context::instance().root_prefix.string() + "/conda-bld",
                "~/conda-bld" };

        std::vector<std::string> local_names;
        local_names.reserve(local_channels.size());
        for (const auto& p : local_channels)
        {
            if (fs::is_directory(p))
            {
                std::string url = path_to_url(p);
                auto channel
                    = Channel::make_simple_channel(m_channel_alias, url, "", LOCAL_CHANNELS_NAME);
                std::string name = channel.name();
                auto res = m_custom_channels.emplace(std::move(name), std::move(channel));
                local_names.push_back(res.first->first);
            }
        }
        m_custom_multichannels.emplace(LOCAL_CHANNELS_NAME, std::move(local_names));

        /*******************
         * SIMPLE CHANNELS *
         *******************/

        // Default local channel
        for (auto& ch : DEFAULT_CUSTOM_CHANNELS)
        {
            m_custom_channels.emplace(
                ch.first, Channel::make_simple_channel(m_channel_alias, ch.second, ch.first));
        }
    }
}  // namespace mamba
