// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <iostream>
#include <regex>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

#include "mamba/core/channel.hpp"
#include "mamba/core/channel_internal.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/package_handling.hpp"
#include "mamba/core/url.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/validate.hpp"


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
    }  // namespace

    /**************************
     * Channel implementation *
     **************************/

    Channel::Channel(const std::string& scheme,
                     const std::string& location,
                     const std::string& name,
                     const std::optional<std::string>& auth,
                     const std::optional<std::string>& token,
                     const std::optional<std::string>& package_filename,
                     const std::optional<std::string>& canonical_name)
        : m_scheme(scheme)
        , m_location(location)
        , m_name(name)
        , m_platforms()
        , m_auth(auth)
        , m_token(token)
        , m_package_filename(package_filename)
        , m_canonical_name(canonical_name)
        , m_repo_checker(base_url(),
                         Context::instance().root_prefix / "etc" / "trusted-repos"
                             / cache_name_from_url(base_url()),
                         Context::instance().root_prefix / "pkgs" / "cache"
                             / cache_name_from_url(base_url()))
    {
        fs::create_directories(m_repo_checker.cache_path());
    }

    const std::string& Channel::scheme() const
    {
        return m_scheme;
    }

    const std::string& Channel::location() const
    {
        return m_location;
    }

    const std::string& Channel::name() const
    {
        return m_name;
    }

    const std::vector<std::string>& Channel::platforms() const
    {
        return m_platforms;
    }

    const std::optional<std::string>& Channel::auth() const
    {
        return m_auth;
    }

    const std::optional<std::string>& Channel::token() const
    {
        return m_token;
    }

    const std::optional<std::string>& Channel::package_filename() const
    {
        return m_package_filename;
    }

    const validate::RepoChecker& Channel::repo_checker() const
    {
        m_repo_checker.generate_index_checker();
        return m_repo_checker;
    }

    const std::string& Channel::canonical_name() const
    {
        if (!m_canonical_name)
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
        return *m_canonical_name;
    }

    std::string Channel::base_url() const
    {
        if (name() == UNKNOWN_CHANNEL)
        {
            return "";
        }
        else
        {
            return concat(scheme(), "://", join_url(location(), name()));
        }
    }

    static std::string build_url(const Channel& c, const std::string& base, bool with_credential)
    {
        if (with_credential && c.auth())
        {
            return concat(c.scheme(), "://", *c.auth(), "@", base);
        }
        else
        {
            return concat(c.scheme(), "://", base);
        }
    }

    static std::optional<std::string> nonempty_str(std::string&& s)
    {
        return s.empty() ? std::optional<std::string>() : std::make_optional(s);
    }

    std::vector<std::string> Channel::urls(bool with_credential) const
    {
        if (package_filename())
        {
            std::string base = location();
            if (with_credential && token())
            {
                base += "/t/" + *token();
            }
            base += "/" + name();
            base += "/" + *package_filename();
            return { { build_url(*this, base, with_credential) } };
        }
        else
        {
            std::vector<std::string> ret;
            for (auto& [_, v] : platform_urls(with_credential))
            {
                ret.emplace_back(v);
            }
            return ret;
        }
    }

    std::vector<std::pair<std::string, std::string>> Channel::platform_urls(
        bool with_credential) const
    {
        std::string base = location();
        if (with_credential && token())
        {
            base += "/t/" + *token();
        }
        base += "/" + name();

        std::vector<std::pair<std::string, std::string>> ret;
        for (const auto& platform : platforms())
        {
            ret.emplace_back(platform, build_url(*this, base + "/" + platform, with_credential));
        }
        return ret;
    }

    std::string Channel::platform_url(std::string platform, bool with_credential) const
    {
        std::string base = location();
        if (with_credential && token())
        {
            base += "/t/" + *token();
        }
        base += "/" + name();

        return build_url(*this, base + "/" + platform, with_credential);
    }

    Channel ChannelInternal::make_simple_channel(const Channel& channel_alias,
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
            auth = channel_alias.auth().value_or("");
            token = channel_alias.token().value_or("");
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
                std::string full_url = concat(scheme, "://", location);
                URLHandler parser(full_url);
                location = rstrip(
                    URLHandler().set_host(parser.host()).set_port(parser.port()).url(), "/");
                name = lstrip(parser.path(), "/");
            }
        }
        name = name != "" ? strip(name, "/") : strip(channel_url, "/");
        return ChannelInternal(scheme,
                               location,
                               name,
                               nonempty_str(std::move(auth)),
                               nonempty_str(std::move(token)),
                               {},
                               nonempty_str(std::string(multi_name)));
    }

    const Channel& ChannelInternal::make_cached_channel(const std::string& value)
    {
        auto res = get_cache().find(value);
        if (res == get_cache().end())
        {
            auto& ctx = Context::instance();

            auto chan = ChannelInternal::from_value(value);
            auto token_base = concat(chan.scheme(), "://", chan.location());
            if (!chan.token())
            {
                auto it = ctx.channel_tokens.find(token_base);
                if (it != ctx.channel_tokens.end())
                {
                    chan.m_token = it->second;
                }
            }
            res = get_cache().insert(std::make_pair(value, std::move(chan))).first;
        }
        return res->second;
    }

    void ChannelInternal::clear_cache()
    {
        get_cache().clear();
    }

    ChannelInternal::ChannelInternal(const std::string& scheme,
                                     const std::string& location,
                                     const std::string& name,
                                     const std::optional<std::string>& auth,
                                     const std::optional<std::string>& token,
                                     const std::optional<std::string>& package_filename,
                                     const std::optional<std::string>& canonical_name)
        : Channel(scheme, location, name, auth, token, package_filename, canonical_name)
    {
    }

    ChannelInternal::cache_type& ChannelInternal::get_cache()
    {
        static cache_type cache;
        return cache;
    }

    static void split_conda_url(const std::string& url,
                                std::string& scheme,
                                std::string& host,
                                std::string& port,
                                std::string& path,
                                std::string& auth,
                                std::string& token,
                                std::string& package_name)
    {
        std::string cleaned_url, extension;
        split_anaconda_token(url, cleaned_url, token);
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

    static channel_configuration read_channel_configuration(const std::string& scheme,
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
                                             channel.auth().value_or(""),
                                             channel.token().value_or(""));
            }
        }

        // Case 5: channel_alias match
        const Channel& ca = ChannelContext::instance().get_channel_alias();
        if (ca.location() != "" && starts_with(url, ca.location()))
        {
            auto name = std::string(strip(url.replace(0u, ca.location().size(), ""), "/"));
            return channel_configuration(
                ca.location(), name, scheme, ca.auth().value_or(""), ca.token().value_or(""));
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

    ChannelInternal ChannelInternal::from_url(const std::string& url)
    {
        std::string scheme, host, port, path, auth, token, package_name;
        split_conda_url(url, scheme, host, port, path, auth, token, package_name);

        auto config = read_channel_configuration(scheme, host, port, path);

        return ChannelInternal(
            config.m_scheme.size() ? config.m_scheme : "https",
            config.m_location,
            config.m_name,
            auth.size() ? std::make_optional(auth) : nonempty_str(std::move(config.m_auth)),
            token.size() ? std::make_optional(token) : nonempty_str(std::move(config.m_token)),
            nonempty_str(std::move(package_name)));
    }

    ChannelInternal ChannelInternal::from_name(const std::string& name)
    {
        std::string tmp_stripped = name;
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
            return ChannelInternal(it->second.scheme(),
                                   it->second.location(),
                                   it->second.name(),
                                   it->second.auth(),
                                   it->second.token(),
                                   it->second.package_filename(),
                                   name);
        }
        else
        {
            const Channel& alias = ChannelContext::instance().get_channel_alias();
            return ChannelInternal(
                alias.scheme(), alias.location(), name, alias.auth(), alias.token());
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

    static std::vector<std::string> take_platforms(std::string& value)
    {
        std::vector<std::string> platforms;
        if (!value.empty())
        {
            if (value[value.size() - 1] == ']')
            {
                const auto end_value = value.find_last_of('[');
                if (end_value != std::string::npos)
                {
                    auto ind = end_value + 1;
                    while (ind < value.size() - 1)
                    {
                        auto end = value.find_first_of(", ]", ind);
                        assert(end != std::string::npos);
                        platforms.emplace_back(value.substr(ind, end - ind));
                        ind = end;
                        while (value[ind] == ',' || value[ind] == ' ')
                            ind++;
                    }

                    value.resize(end_value);
                }
            }
        }

        if (platforms.empty())
        {
            platforms = Context::instance().platforms();
        }
        return platforms;
    }


    ChannelInternal ChannelInternal::from_value(const std::string& in_value)
    {
        if (INVALID_CHANNELS.count(in_value) > 0)
        {
            return ChannelInternal("", "", UNKNOWN_CHANNEL, "");
        }

        std::string value = in_value;
        auto platforms = take_platforms(value);

        auto chan = has_scheme(value)        ? ChannelInternal::from_url(fix_win_path(value))
                    : is_path(value)         ? ChannelInternal::from_url(path_to_url(value))
                    : is_package_file(value) ? ChannelInternal::from_url(fix_win_path(value))
                                             : ChannelInternal::from_name(value);

        chan.m_platforms = std::move(platforms);

        return chan;
    }

    ChannelInternal ChannelInternal::from_alias(const std::string& scheme,
                                                const std::string& location,
                                                const std::optional<std::string>& auth,
                                                const std::optional<std::string>& token)
    {
        return ChannelInternal(scheme, location, "<alias>", auth, token);
    }

    /************************************
     * utility functions implementation *
     ************************************/

    const Channel& make_channel(const std::string& value)
    {
        return ChannelInternal::make_cached_channel(value);
    }

    static void append_channel_urls(const std::string name,
                                    bool with_credential,
                                    std::vector<std::string>& result,
                                    std::set<std::string>& control)
    {
        // this checks if the channel is already in our channel_urls list
        bool ret = !control.insert(name).second;
        if (ret)
            return;

        std::vector<std::string> urls = make_channel(name).urls(with_credential);
        std::copy(urls.begin(), urls.end(), std::back_inserter(result));
    }

    std::vector<const Channel*> get_channels(const std::vector<std::string>& channel_names)
    {
        std::set<const Channel*> added;
        std::vector<const Channel*> result;
        for (auto name : channel_names)
        {
            std::string platform_spec;
            auto platform_spec_ind = name.find("[");
            if (platform_spec_ind != std::string::npos)
            {
                platform_spec = name.substr(platform_spec_ind);
                name = name.substr(0, platform_spec_ind);
            }

            auto add_channel = [&](const std::string& name) {
                auto channel = &make_channel(name + platform_spec);
                if (added.insert(channel).second)
                {
                    result.push_back(channel);
                }
            };
            auto multi_iter = ChannelContext::instance().get_custom_multichannels().find(name);
            if (multi_iter != ChannelContext::instance().get_custom_multichannels().end())
            {
                for (const auto& n : multi_iter->second)
                {
                    add_channel(n);
                }
            }
            else
            {
                add_channel(name);
            }
        }
        return result;
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

    void ChannelContext::reset()
    {
        m_channel_alias = build_channel_alias();
        m_custom_channels.clear();
        m_custom_multichannels.clear();
        m_whitelist_channels.clear();
        init_custom_channels();
        ChannelInternal::clear_cache();
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
        return ChannelInternal::from_alias(
            scheme, location, nonempty_str(std::move(auth)), nonempty_str(std::move(token)));
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
            auto channel = ChannelInternal::make_simple_channel(
                m_channel_alias, url, "", DEFAULT_CHANNELS_NAME);
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
                auto channel = ChannelInternal::make_simple_channel(
                    m_channel_alias, url, "", LOCAL_CHANNELS_NAME);
                std::string name = channel.name();
                auto res = m_custom_channels.emplace(std::move(name), std::move(channel));
                local_names.push_back(res.first->first);
            }
        }
        m_custom_multichannels.emplace(LOCAL_CHANNELS_NAME, std::move(local_names));

        const auto& context_custom_channels = Context::instance().custom_channels;
        for (const auto& [n, p] : context_custom_channels)
        {
            std::string url = p;
            if (!starts_with(url, "http"))
                url = path_to_url(url);

            auto channel
                = ChannelInternal::make_simple_channel(m_channel_alias, join_url(url, n), "", n);
            auto res = m_custom_channels.emplace(n, std::move(channel));
        }

        /*******************
         * SIMPLE CHANNELS *
         *******************/

        // Default local channel
        for (auto& ch : DEFAULT_CUSTOM_CHANNELS)
        {
            m_custom_channels.emplace(
                ch.first,
                ChannelInternal::make_simple_channel(m_channel_alias, ch.second, ch.first));
        }
    }

    void load_tokens()
    {
        auto& ctx = Context::instance();
        std::vector<fs::path> found_tokens;

        for (const auto& loc : ctx.token_locations)
        {
            auto px = env::expand_user(loc);
            if (!fs::exists(px) || !fs::is_directory(px))
            {
                continue;
            }
            for (const auto& entry : fs::directory_iterator(px))
            {
                if (ends_with(entry.path().filename().string(), ".token"))
                {
                    found_tokens.push_back(entry.path());
                    std::string token_url = decode_url(entry.path().filename());

                    // anaconda client writes out a token for https://api.anaconda.org...
                    // but we need the token for https://conda.anaconda.org
                    // conda does the same
                    std::size_t api_pos = token_url.find("://api.");
                    if (api_pos != std::string::npos)
                    {
                        token_url.replace(api_pos, 7, "://conda.");
                    }

                    // cut ".token" ending
                    token_url = token_url.substr(0, token_url.size() - 6);

                    std::string token_content = read_contents(entry.path());
                    ctx.channel_tokens[token_url] = token_content;
                    LOG_WARNING << "Found token for " << token_url;
                    LOG_DEBUG << "Token content: " << token_content;
                }
            }
        }
    }

}  // namespace mamba
