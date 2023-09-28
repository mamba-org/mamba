// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <iostream>
#include <set>
#include <utility>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"
#include "mamba/util/url_manip.hpp"


namespace mamba
{
    // Constants used by Channel and ChannelContext
    namespace
    {
        const std::map<std::string, std::string> DEFAULT_CUSTOM_CHANNELS = {
            { "pkgs/pro", "https://repo.anaconda.com" }
        };
        const char UNKNOWN_CHANNEL[] = "<unknown>";

        const std::set<std::string> INVALID_CHANNELS = { "<unknown>",
                                                         "None:///<unknown>",
                                                         "None",
                                                         "",
                                                         ":///<unknown>" };

        const char LOCAL_CHANNELS_NAME[] = "local";
        const char DEFAULT_CHANNELS_NAME[] = "defaults";
    }  // namespace

    // Specific functions, used only in this file
    namespace
    {
        std::optional<std::string> nonempty_str(std::string&& s)
        {
            return s.empty() ? std::optional<std::string>() : std::make_optional(s);
        }

        // Channel configuration
        struct channel_configuration
        {
            std::string location;
            std::string name;
            std::string scheme;
            std::string user;
            std::string password;
            std::string token;
        };

        channel_configuration read_channel_configuration(
            ChannelContext& channel_context,
            const std::string& scheme,
            const std::string& host,
            const std::string& port,
            const std::string& path
        )
        {
            auto spath = std::string(util::rstrip(path, '/'));
            std::string url = [&]()
            {
                auto parsed_url = util::URL();
                parsed_url.set_scheme(scheme);
                parsed_url.set_host(host);
                parsed_url.set_port(port);
                parsed_url.set_path(spath);
                return parsed_url.pretty_str(util::URL::StripScheme::yes);
            }();

            // Case 1: No path given, channel name is ""
            if (spath.empty())
            {
                auto l_url = util::URL();
                l_url.set_host(host);
                l_url.set_port(port);
                return channel_configuration{
                    /* .location= */ l_url
                        .pretty_str(util::URL::StripScheme::yes, /* rstrip_path= */ '/'),
                    /* .name= */ "",
                    /* .scheme= */ scheme,
                    /* .user= */ "",
                    /* .password= */ "",
                    /* .token= */ "",
                };
            }

            // Case 2: migrated_custom_channels not implemented yet
            // Case 3: migrated_channel_aliases not implemented yet

            // Case 4: custom_channels matches
            const auto& custom_channels = channel_context.get_custom_channels();
            for (const auto& ca : custom_channels)
            {
                const Channel& channel = ca.second;
                std::string test_url = util::join_url(channel.location(), channel.name());
                if (vector_is_prefix(util::split(test_url, "/"), util::split(url, "/")))
                {
                    auto subname = std::string(util::strip(url.replace(0u, test_url.size(), ""), '/'));

                    return channel_configuration{
                        /* .location= */ channel.location(),
                        /* .name= */ util::join_url(channel.name(), subname),
                        /* .scheme= */ scheme,
                        /* .user= */ channel.user().value_or(""),
                        /* .password= */ channel.password().value_or(""),
                        /* .token= */ channel.token().value_or(""),
                    };
                }
            }

            // Case 5: channel_alias match
            const Channel& ca = channel_context.get_channel_alias();
            if (!ca.location().empty() && util::starts_with(url, ca.location()))
            {
                auto name = std::string(util::strip(url.replace(0u, ca.location().size(), ""), '/'));
                return channel_configuration{
                    /* .location= */ ca.location(),
                    /* .name= */ name,
                    /* .scheme= */ scheme,
                    /* .user= */ ca.user().value_or(""),
                    /* .password= */ ca.password().value_or(""),
                    /* .token= */ ca.token().value_or(""),
                };
            }

            // Case 6: not-otherwise-specified file://-type urls
            if (host.empty() || ((host == util::URL::localhost) && port.empty()))
            {
                auto sp = util::rsplit(url, "/", 1);
                return channel_configuration{
                    /* .location= */ sp[0].size() ? sp[0] : "/",
                    /* .name= */ sp[1],
                    /* .scheme= */ "file",
                    /* .user= */ "",
                    /* .password= */ "",
                    /* .token= */ "",
                };
            }

            // Case 7: fallback, channel_location = host:port and channel_name = path
            spath = util::lstrip(spath, '/');
            auto location = util::URL();
            location.set_host(host);
            location.set_port(port);
            return channel_configuration{
                /* .location= */ location.pretty_str(util::URL::StripScheme::yes, /* rstrip_path= */ '/'),
                /* .name= */ spath,
                /* .scheme= */ scheme,
                /* .user= */ "",
                /* .password= */ "",
                /* .token= */ "",
            };
        }

        util::flat_set<std::string> take_platforms(const Context& context, std::string& value)
        {
            util::flat_set<std::string> platforms;
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
                            platforms.insert(value.substr(ind, end - ind));
                            ind = end;
                            while (value[ind] == ',' || value[ind] == ' ')
                            {
                                ind++;
                            }
                        }

                        value.resize(end_value);
                    }
                }
                // This is required because a channel can be instantiated from an URL
                // that already contains the platform
                else
                {
                    std::string platform = "";
                    util::split_platform(get_known_platforms(), value, context.platform, value, platform);
                    if (!platform.empty())
                    {
                        platforms.insert(std::move(platform));
                    }
                }
            }

            if (platforms.empty())
            {
                for (const auto& plat : context.platforms())
                {
                    platforms.insert(plat);
                }
            }
            return platforms;
        }
    }  // namespace

    std::vector<std::string> get_known_platforms()
    {
        auto plats = specs::known_platform_names();
        return { plats.begin(), plats.end() };
    }

    /**************************
     * Channel implementation *
     **************************/

    Channel::Channel(
        std::string_view scheme,
        std::string location,
        std::string name,
        std::string canonical_name,
        std::string_view user,
        std::string_view password,
        std::string_view token,
        std::string_view package_filename
    )
        : m_url()
        , m_location(std::move(location))
        , m_name(std::move(name))
        , m_canonical_name(std::move(canonical_name))
        , m_platforms()
    {
        if (m_name != UNKNOWN_CHANNEL)
        {
            if (scheme == "file")
            {
                m_url.set_path(m_location);
            }
            else
            {
                m_url = specs::CondaURL::parse(m_location);
                if (!token.empty())
                {
                    m_url.set_token(token);
                }
                m_url.set_user(user);
                m_url.set_password(password);
            }
            m_url.set_scheme(scheme);
            m_url.append_path(m_name);
            if (!package_filename.empty())
            {
                m_url.set_package(package_filename);
            }
        }
    }

    Channel::~Channel() = default;

    const std::string& Channel::scheme() const
    {
        return m_url.scheme();
    }

    const std::string& Channel::location() const
    {
        return m_location;
    }

    const std::string& Channel::name() const
    {
        return m_name;
    }

    const util::flat_set<std::string>& Channel::platforms() const
    {
        return m_platforms;
    }

    std::optional<std::string> Channel::auth() const
    {
        return nonempty_str(m_url.authentication());
    }

    std::optional<std::string> Channel::user() const
    {
        return nonempty_str(m_url.user());
    }

    std::optional<std::string> Channel::password() const
    {
        return nonempty_str(m_url.password());
    }

    std::optional<std::string> Channel::token() const
    {
        return nonempty_str(std::string(m_url.token()));
    }

    std::optional<std::string> Channel::package_filename() const
    {
        return nonempty_str(m_url.package());
    }

    const validation::RepoChecker&
    Channel::repo_checker(Context& context, MultiPackageCache& caches) const
    {
        if (p_repo_checker == nullptr)
        {
            p_repo_checker = std::make_unique<validation::RepoChecker>(
                context,
                util::rsplit(base_url(), "/", 1).front(),
                context.prefix_params.root_prefix / "etc" / "trusted-repos"
                    / util::cache_name_from_url(base_url()),
                caches.first_writable_path() / "cache" / util::cache_name_from_url(base_url())
            );

            fs::create_directories(p_repo_checker->cache_path());
            p_repo_checker->generate_index_checker();
        }

        return *p_repo_checker;
    }

    const std::string& Channel::canonical_name() const
    {
        return m_canonical_name;
    }

    std::string Channel::base_url() const
    {
        if (name() == UNKNOWN_CHANNEL)
        {
            return "";
        }
        else
        {
            return util::concat_scheme_url(scheme(), util::join_url(location(), name()));
        }
    }

    util::flat_set<std::string> Channel::urls(bool with_credential) const
    {
        if (package_filename())
        {
            std::string base = location();
            if (with_credential && token())
            {
                base = util::join_url(base, "t", *token());
            }

            std::string platform = m_platforms[0];
            return { { util::build_url(
                auth(),
                scheme(),
                util::join_url(base, name(), platform, *package_filename()),
                with_credential
            ) } };
        }
        else
        {
            auto out = util::flat_set<std::string>{};
            for (auto& [_, v] : platform_urls(with_credential))
            {
                out.insert(v);
            }
            return out;
        }
    }

    util::flat_set<std::pair<std::string, std::string>>
    Channel::platform_urls(bool with_credential) const
    {
        std::string base = location();
        if (with_credential && token())
        {
            base = util::join_url(base, "t", *token());
        }

        auto out = util::flat_set<std::pair<std::string, std::string>>{};
        for (const auto& platform : platforms())
        {
            out.insert({
                platform,
                util::build_url(auth(), scheme(), util::join_url(base, name(), platform), with_credential),
            });
        }
        return out;
    }

    std::string Channel::platform_url(std::string platform, bool with_credential) const
    {
        std::string base = location();
        if (with_credential && token())
        {
            base = util::join_url(base, "t", *token());
        }
        return util::build_url(auth(), scheme(), util::join_url(base, name(), platform), with_credential);
    }

    bool operator==(const Channel& lhs, const Channel& rhs)
    {
        return lhs.location() == rhs.location() && lhs.name() == rhs.name();
    }

    bool operator!=(const Channel& lhs, const Channel& rhs)
    {
        return !(lhs == rhs);
    }

    /*********************************
     * ChannelContext implementation *
     *********************************/

    Channel ChannelContext::make_simple_channel(
        const Channel& channel_alias,
        const std::string& channel_url,
        const std::string& channel_name,
        const std::string& channel_canonical_name
    )
    {
        if (!util::url_has_scheme(channel_url))
        {
            return Channel(
                /*  scheme= */ channel_alias.scheme(),
                /*  location= */ channel_alias.location(),
                /*  name= */ std::string(util::strip(channel_name.empty() ? channel_url : channel_name, '/')),
                /*  canonical_name= */ channel_canonical_name,
                /*  user= */ channel_alias.user().value_or(""),
                /*  password= */ channel_alias.password().value_or(""),
                /*  token= */ channel_alias.token().value_or(""),
                /*  package_filename= */ {}
            );
        }


        auto url = specs::CondaURL::parse(channel_url);
        std::string token = std::string(url.token());
        url.clear_token();
        std::string user = url.user();  // % encoded
        url.clear_user();
        std::string password = url.password();  // % encoded
        url.clear_password();
        std::string location = url.pretty_str(specs::CondaURL::StripScheme::yes, '/');

        std::string name(channel_name);
        if (name.empty())
        {
            if (!channel_alias.location().empty()
                && util::starts_with(location, channel_alias.location()))
            {
                name = location;
                name.replace(0u, channel_alias.location().size(), "");
                location = channel_alias.location();
            }
            else if (url.scheme() == "file")
            {
                const auto pos = location.rfind('/');
                name = location.substr(pos + 1);
                location = location.substr(0, pos);
            }
            else
            {
                location = util::concat(url.host(), url.port().empty() ? "" : ":", url.port());
                name = url.path();
            }
        }
        name = util::strip(name.empty() ? channel_url : name, '/');
        return Channel(
            /*  scheme= */ url.scheme(),
            /*  location= */ location,
            /*  name= */ name,
            /*  canonical_name= */ channel_canonical_name,
            /*  user= */ user,
            /*  password= */ password,
            /*  token= */ token,
            /*  package_filename= */ {}
        );
    }

    Channel ChannelContext::from_url(std::string_view url_str)
    {
        auto url = specs::CondaURL::parse(url_str);
        std::string package_name = url.package();
        url.clear_package();
        std::string token = std::string(url.token());
        url.clear_token();

        auto config = read_channel_configuration(*this, url.scheme(), url.host(), url.port(), url.path());

        auto res_scheme = !config.scheme.empty() ? config.scheme : "https";
        std::string canonical_name;

        const auto& custom_channels = get_custom_channels();
        if ((custom_channels.find(config.name) != custom_channels.end())
            || (config.location == get_channel_alias().location()))
        {
            canonical_name = config.name;
        }
        else
        {
            canonical_name = util::concat_scheme_url(
                res_scheme,
                util::join_url(config.location, config.name)
            );
        }

        std::string user = url.user();          // % encoded
        std::string password = url.password();  // % encoded
        return Channel(
            /*  scheme= */ res_scheme,
            /*  location= */ config.location,
            /*  name= */ config.name,
            /*  canonical_name= */ canonical_name,
            /*  user= */ user.empty() ? config.user : user,
            /*  password= */ password.empty() ? config.password : password,
            /*  token= */ token.empty() ? config.token : token,
            /*  package_filename= */ package_name
        );
    }

    Channel ChannelContext::from_name(const std::string& name)
    {
        std::string tmp_stripped = name;
        const auto& custom_channels = get_custom_channels();
        const auto it_end = custom_channels.end();
        auto it = custom_channels.find(tmp_stripped);
        while (it == it_end)
        {
            const auto pos = tmp_stripped.rfind("/");
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
            // we can have a channel like
            // testchannel: https://server.com/private/testchannel
            // where `name == private/testchannel` and we need to join the remaining label part
            // of the channel (e.g. -c testchannel/mylabel/xyz)
            // needs to result in `name = private/testchannel/mylabel/xyz`
            std::string combined_name = it->second.name();
            if (combined_name != name)
            {
                // Find common string between `name` and `combined_name`
                auto common_str = util::get_common_parts(combined_name, name, "/");
                // Combine names properly
                if (common_str.empty())
                {
                    combined_name += "/" + name;
                }
                else
                {
                    // NOTE We assume that the `common_str`, if not empty, is necessarily at the
                    // beginning of `name` and at the end of `combined_name` (I don't know about
                    // other use cases for now)
                    combined_name += name.substr(common_str.size());
                }
            }

            return Channel(
                /*  scheme= */ it->second.scheme(),
                /*  location= */ it->second.location(),
                /*  name= */ combined_name,
                /*  canonical_name= */ name,
                /*  user= */ it->second.user().value_or(""),
                /*  password= */ it->second.password().value_or(""),
                /*  token= */ it->second.token().value_or(""),
                /*  package_filename= */ it->second.package_filename().value_or("")
            );
        }
        else
        {
            const Channel& alias = get_channel_alias();
            return Channel(
                /*  scheme= */ alias.scheme(),
                /*  location= */ alias.location(),
                /*  name= */ name,
                /*  canonical_name= */ name,
                /*  user= */ alias.user().value_or(""),
                /*  password= */ alias.password().value_or(""),
                /*  token= */ alias.token().value_or("")
            );
        }
    }

    Channel ChannelContext::from_value(const std::string& in_value)
    {
        if (INVALID_CHANNELS.count(in_value) > 0)
        {
            return Channel(
                /*  scheme= */ "",
                /*  location= */ "",
                /*  name= */ UNKNOWN_CHANNEL,
                /*  canonical_name= */ ""
            );
        }

        std::string value = in_value;
        auto platforms = take_platforms(m_context, value);

        auto chan = util::url_has_scheme(value)     ? from_url(fix_win_path(value))
                    : util::is_explicit_path(value) ? from_url(util::path_or_url_to_url(value))
                    : specs::has_archive_extension(value) ? from_url(fix_win_path(value))
                                                          : from_name(value);

        chan.m_platforms = std::move(platforms);

        return chan;
    }

    Channel ChannelContext::from_alias(std::string_view alias)
    {
        auto url = specs::CondaURL::parse(alias);

        std::string token = std::string(url.token());
        std::string user = url.user();  // % encoded
        url.clear_user();
        std::string password = url.password();  // % encoded
        url.clear_password();
        url.clear_token();

        return Channel(
            /*  scheme= */ url.scheme(),
            /*  location= */ url.pretty_str(specs::CondaURL::StripScheme::yes, '/'),
            /*  name= */ "<alias>",
            /*  canonical_name= */ "<alias>",
            /*  user= */ user,
            /*  password= */ password,
            /*  token= */ token
        );
    }


    const Channel& ChannelContext::make_channel(const std::string& value)
    {
        auto res = m_channel_cache.find(value);
        if (res == m_channel_cache.end())
        {
            auto chan = from_value(value);
            if (!chan.token())
            {
                const auto& with_channel = util::join_url(
                    chan.location(),
                    chan.name() == UNKNOWN_CHANNEL ? "" : chan.name()
                );
                const auto& without_channel = chan.location();
                for (const auto& auth : { with_channel, without_channel })
                {
                    const auto& authentication_info = m_context.authentication_info();
                    auto it = authentication_info.find(auth);
                    if (it != authentication_info.end()
                        && std::holds_alternative<specs::CondaToken>(it->second))
                    {
                        chan.m_url.set_token(std::get<specs::CondaToken>(it->second).token);
                        break;
                    }
                    else if (it != authentication_info.end() && std::holds_alternative<specs::BasicHTTPAuthentication>(it->second))
                    {
                        const auto& l_auth = std::get<specs::BasicHTTPAuthentication>(it->second);
                        chan.m_url.set_user(l_auth.user);
                        chan.m_url.set_password(l_auth.password);
                        break;
                    }
                }
            }
            res = m_channel_cache.insert(std::make_pair(value, std::move(chan))).first;
        }
        return res->second;
    }

    std::vector<const Channel*>
    ChannelContext::get_channels(const std::vector<std::string>& channel_names)
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

            auto add_channel = [&](const std::string& lname)
            {
                auto* channel = &make_channel(lname + platform_spec);
                if (added.insert(channel).second)
                {
                    result.push_back(channel);
                }
            };
            auto multi_iter = get_custom_multichannels().find(name);
            if (multi_iter != get_custom_multichannels().end())
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

    ChannelContext::ChannelContext(Context& context)
        : m_context(context)
        , m_channel_alias(from_alias(m_context.channel_alias))
    {
        init_custom_channels();
    }

    ChannelContext::~ChannelContext() = default;

    void ChannelContext::init_custom_channels()
    {
        /******************
         * MULTI CHANNELS *
         ******************/

        // Default channels
        auto& default_channels = m_context.default_channels;
        std::vector<std::string> default_names(default_channels.size());
        auto default_name_iter = default_names.begin();
        for (auto& url : default_channels)
        {
            auto channel = make_simple_channel(m_channel_alias, url, "", DEFAULT_CHANNELS_NAME);
            std::string name = channel.name();
            auto res = m_custom_channels.emplace(std::move(name), std::move(channel));
            *default_name_iter++ = res.first->first;
        }
        m_custom_multichannels.emplace(DEFAULT_CHANNELS_NAME, std::move(default_names));

        // Local channels
        std::vector<std::string> local_channels = { m_context.prefix_params.target_prefix
                                                        / "conda-bld",
                                                    m_context.prefix_params.root_prefix / "conda-bld",
                                                    env::home_directory() / "conda-bld" };

        bool at_least_one_local_dir = false;
        std::vector<std::string> local_names;
        local_names.reserve(local_channels.size());
        for (const auto& p : local_channels)
        {
            if (fs::is_directory(p))
            {
                at_least_one_local_dir = true;
                std::string url = util::path_or_url_to_url(p);
                auto channel = make_simple_channel(m_channel_alias, url, "", LOCAL_CHANNELS_NAME);
                std::string name = channel.name();
                auto res = m_custom_channels.emplace(std::move(name), std::move(channel));
                local_names.push_back(res.first->first);
            }
        }

        // Throw if `-c local` is given but none of the specified `local_channels` are found
        if (!at_least_one_local_dir
            && std::find(m_context.channels.begin(), m_context.channels.end(), LOCAL_CHANNELS_NAME)
                   != m_context.channels.end())
        {
            throw std::runtime_error(
                "No 'conda-bld' directory found in target prefix, root prefix or home directories!"
            );
        }

        m_custom_multichannels.emplace(LOCAL_CHANNELS_NAME, std::move(local_names));

        const auto& context_custom_channels = m_context.custom_channels;
        for (const auto& [n, p] : context_custom_channels)
        {
            std::string url = p;
            if (!util::starts_with(url, "http"))
            {
                url = util::path_or_url_to_url(url);
            }

            auto channel = make_simple_channel(m_channel_alias, url, n, n);
            m_custom_channels.emplace(n, std::move(channel));
        }

        auto& multichannels = m_context.custom_multichannels;
        for (auto& [multichannelname, urllist] : multichannels)
        {
            std::vector<std::string> names(urllist.size());
            auto name_iter = names.begin();
            for (auto& url : urllist)
            {
                auto channel = make_simple_channel(m_channel_alias, url, "", multichannelname);
                std::string name = channel.name();
                m_custom_channels.emplace(std::move(name), std::move(channel));
                *name_iter++ = url;
            }
            m_custom_multichannels.emplace(multichannelname, std::move(names));
        }

        /*******************
         * SIMPLE CHANNELS *
         *******************/

        // Default local channel
        for (auto& ch : DEFAULT_CUSTOM_CHANNELS)
        {
            m_custom_channels.emplace(
                ch.first,
                make_simple_channel(m_channel_alias, ch.second, ch.first, ch.first)
            );
        }
    }
}  // namespace mamba
