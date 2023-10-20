// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <set>
#include <utility>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/package_cache.hpp"
#include "mamba/core/validate.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url.hpp"
#include "mamba/util/url_manip.hpp"


namespace mamba
{
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

        std::optional<std::string> nonempty_str(std::string&& s)
        {
            return s.empty() ? std::optional<std::string>() : std::make_optional(s);
        }
    }

    std::vector<std::string> get_known_platforms()
    {
        auto plats = specs::known_platform_names();
        return { plats.begin(), plats.end() };
    }

    /**************************
     * Channel implementation *
     **************************/

    Channel::Channel(
        specs::CondaURL url,
        std::string location,
        std::string name,
        std::string canonical_name,
        util::flat_set<std::string> platforms
    )
        : m_url(std::move(url))
        , m_location(std::move(location))
        , m_name(std::move(name))
        , m_canonical_name(std::move(canonical_name))
        , m_platforms(std::move(platforms))
    {
    }

    Channel::~Channel() = default;

    const specs::CondaURL& Channel::url() const
    {
        return m_url;
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
        return url().str(specs::CondaURL::Credentials::Remove);
    }

    util::flat_set<std::string> Channel::urls(bool with_credential) const
    {
        if (!url().package().empty())
        {
            return { url().str(
                with_credential ? specs::CondaURL::Credentials::Show
                                : specs::CondaURL::Credentials::Remove
            ) };
        }

        auto out = util::flat_set<std::string>{};
        for (auto& [_, v] : platform_urls(with_credential))
        {
            out.insert(v);
        }
        return out;
    }

    util::flat_set<std::pair<std::string, std::string>>
    Channel::platform_urls(bool with_credential) const
    {
        if (!url().package().empty())
        {
            return {};
        }

        auto out = util::flat_set<std::pair<std::string, std::string>>{};
        for (const auto& platform : platforms())
        {
            out.insert({ platform, platform_url(platform, with_credential) });
        }
        return out;
    }

    std::string Channel::platform_url(std::string_view platform, bool with_credential) const
    {
        auto cred = with_credential ? specs::CondaURL::Credentials::Show
                                    : specs::CondaURL::Credentials::Remove;

        if (!url().package().empty())
        {
            return url().str(cred);
        }
        return (url() / platform).str(cred);
    }

    /*********************************
     * ChannelContext implementation *
     *********************************/

    Channel ChannelContext::make_simple_channel(
        const specs::CondaURL& channel_alias,
        const std::string& channel_url,
        const std::string& channel_name,
        const std::string& channel_canonical_name
    )
    {
        if (!util::url_has_scheme(channel_url))
        {
            const auto& alias = get_channel_alias();
            auto url = alias;
            auto name = std::string(util::strip(channel_name.empty() ? channel_url : channel_name, '/')
            );
            url.append_path(channel_url);
            return Channel(
                /* url= */ std::move(url),
                /*  location= */ alias.pretty_str(specs::CondaURL::StripScheme::yes, '/', specs::CondaURL::Credentials::Remove),
                /*  name= */ std::move(name),
                /*  canonical_name= */ channel_canonical_name
            );
        }

        auto url = specs::CondaURL::parse(channel_url);
        std::string location = url.pretty_str(
            specs::CondaURL::StripScheme::yes,
            '/',
            specs::CondaURL::Credentials::Remove
        );
        std::string name(channel_name);

        if (channel_name.empty())
        {
            auto ca_location = channel_alias.pretty_str(
                specs::CondaURL::StripScheme::yes,
                '/',
                specs::CondaURL::Credentials::Remove
            );

            if (util::starts_with(location, ca_location))
            {
                name = std::string(util::strip(util::remove_prefix(location, ca_location), '/'));
                location = std::move(ca_location);
            }
            else if (url.scheme() == "file")
            {
                const auto pos = location.rfind('/');
                name = location.substr(pos + 1);
                location = location.substr(0, pos);
            }
            else
            {
                location = url.authority(specs::CondaURL::Credentials::Remove);
                name = url.path_without_token();
            }
        }
        else
        {
            url.append_path(channel_name);
        }

        name = util::strip(name.empty() ? channel_url : name, '/');
        return Channel(
            /* url = */ std::move(url),
            /* location= */ std::move(location),
            /* name= */ std::move(name),
            /* canonical_name= */ channel_canonical_name
        );
    }

    namespace
    {
        auto url_match(const specs::CondaURL& registered, const specs::CondaURL& candidate) -> bool
        {
            using Decode = typename specs::CondaURL::Decode;

            // Not checking users, passwords, and tokens
            return /**/
                // Defaulted scheme matches all, otherwise schemes must be the same
                (registered.scheme_is_defaulted() || (registered.scheme() == candidate.scheme()))
                // Hosts must always be the same
                && (registered.host(Decode::no) == candidate.host(Decode::no))
                // Different ports are considered different channels
                && (registered.port() == candidate.port())
                // Registered path must be a prefix
                && util::path_is_prefix(
                    registered.path_without_token(Decode::no),
                    candidate.path_without_token(Decode::no)
                );
        }

        void set_fallback_credential_from_url(specs::CondaURL& url, const specs::CondaURL& from)
        {
            using Decode = typename specs::CondaURL::Decode;
            using Encode = typename specs::CondaURL::Encode;

            if (!url.has_user() && from.has_user())
            {
                url.set_user(from.user(Decode::no), Encode::no);
                url.set_password(from.password(Decode::no), Encode::no);
            }
            if (!url.has_token() && from.has_token())
            {
                url.set_token(from.token());
            }
        }

        void
        set_fallback_credential_from_auth(specs::CondaURL& url, const specs::AuthenticationInfo& auth)
        {
            std::visit(
                [&](const auto& info)
                {
                    using Info = std::decay_t<decltype(info)>;
                    if constexpr (std::is_same_v<Info, specs::BasicHTTPAuthentication>)
                    {
                        if (!url.has_user() && !url.has_password())
                        {
                            url.set_user(info.user, specs::CondaURL::Encode::yes);
                            url.set_password(info.password, specs::CondaURL::Encode::yes);
                        }
                    }
                    else if constexpr (std::is_same_v<Info, specs::CondaToken>)
                    {
                        if (!url.has_token())
                        {
                            url.set_token(info.token);
                        }
                    }
                },
                auth
            );
        }

        void
        set_fallback_credential_from_db(specs::CondaURL& url, const specs::AuthenticationDataBase& db)
        {
            if (!url.has_token() || !url.has_user() || !url.has_password())
            {
                const auto key = url.pretty_str(
                    specs::CondaURL::StripScheme::yes,
                    '/',
                    specs::CondaURL::Credentials::Remove
                );
                if (auto it = db.find_compatible(key); it != db.end())
                {
                    set_fallback_credential_from_auth(url, it->second);
                }
            }
        }

        auto rsplit_once(std::string_view str, char sep)
        {
            auto [head, tail] = util::rstrip_if_parts(str, [sep](char c) { return c != sep; });
            if (head.empty())
            {
                return std::array{ head, tail };
            }
            return std::array{ head.substr(0, head.size() - 1), tail };
        }

        auto
        make_platforms(util::flat_set<std::string> filters, const std::vector<std::string>& defaults)
        {
            if (filters.empty())
            {
                for (const auto& plat : defaults)
                {
                    filters.insert(plat);
                }
            }
            return filters;
        };
    }

    Channel ChannelContext::from_any_path(specs::ChannelSpec&& spec)
    {
        auto uri = specs::CondaURL::parse(util::path_or_url_to_url(spec.location()));

        auto path = uri.pretty_path();
        auto [parent, current] = rsplit_once(path, '/');
        for (const auto& [canonical_name, chan] : get_custom_channels())
        {
            if (url_match(chan.url(), uri))
            {
                return Channel(
                    /* url= */ std::move(uri),
                    /* location= */ chan.url().pretty_str(specs::CondaURL::StripScheme::yes),
                    /* name= */ std::string(util::rstrip(parent, '/')),
                    /* canonical_name= */ std::string(canonical_name)
                );
            }
        }

        if (const auto& ca = get_channel_alias(); url_match(ca, uri))
        {
            auto name = util::strip(util::remove_prefix(uri.path(), ca.path()), '/');
            return Channel(
                /* url= */ std::move(uri),
                /*  location= */ ca.pretty_str(specs::CondaURL::StripScheme::yes),
                /*  name= */ std::string(name),
                /*  canonical_name= */ std::string(name)
            );
        }

        auto canonical_name = uri.pretty_str();
        return Channel(
            /* url= */ std::move(uri),
            /* location= */ std::string(util::rstrip(parent, '/')),
            /* name= */ std::string(util::rstrip(current, '/')),
            /* canonical_name= */ std::move(canonical_name)
        );
    }

    Channel ChannelContext::from_package_path(specs::ChannelSpec&& spec)
    {
        assert(spec.type() == specs::ChannelSpec::Type::PackagePath);
        return from_any_path(std::move(spec));
    }

    Channel ChannelContext::from_path(specs::ChannelSpec&& spec)
    {
        assert(spec.type() == specs::ChannelSpec::Type::Path);
        auto platforms = make_platforms(spec.clear_platform_filters(), m_context.platforms());
        auto chan = from_any_path(std::move(spec));
        chan.m_platforms = std::move(platforms);
        return chan;
    }

    Channel ChannelContext::from_any_url(specs::ChannelSpec&& spec)
    {
        assert(util::url_has_scheme(spec.location()));
        auto url = specs::CondaURL::parse(spec.location());
        assert(url.scheme() != "file");

        using StripScheme = typename specs::CondaURL::StripScheme;
        using Credentials = typename specs::CondaURL::Credentials;

        std::string default_location = url.pretty_str(StripScheme::yes, '/', Credentials::Remove);

        for (const auto& [canonical_name, chan] : get_custom_channels())
        {
            if (url_match(chan.url(), url))
            {
                std::string location = chan.location();
                // TODO cannot change all the locations at once since they are used in from_name
                // std::string location = chan.url().pretty_str(StripScheme::yes, '/',
                // Credentials::Remove);
                std::string name = std::string(
                    util::strip(util::remove_prefix(default_location, location), '/')
                );
                set_fallback_credential_from_url(url, chan.url());
                return Channel(
                    /* url= */ std::move(url),
                    /* location= */ std::move(location),
                    /* name= */ std::move(name),
                    /* canonical_name= */ std::string(canonical_name)
                );
            }
        }

        if (const auto& ca = get_channel_alias(); url_match(ca, url))
        {
            auto location = ca.pretty_str(StripScheme::yes, '/', Credentials::Remove);
            // Overridding url scheme since chan_url could have been defaulted
            auto name = std::string(util::strip(util::remove_prefix(default_location, location), '/'));
            set_fallback_credential_from_url(url, ca);
            return Channel(
                /*..url= */ std::move(url),
                /* location= */ std::move(location),
                /* name= */ name,
                /* canonical_name= */ name
            );
        }

        auto name = std::string(util::strip(url.path_without_token(), '/'));
        auto location = url.authority(Credentials::Remove);
        auto canonical_name = url.pretty_str(StripScheme::no, '/', Credentials::Remove);
        return Channel(
            /* url= */ std::move(url),
            /* location= */ std::move(location),
            /* name= */ std::move(name),
            /* canonical_name= */ std::move(canonical_name)
        );
    }

    Channel ChannelContext::from_package_url(specs::ChannelSpec&& spec)
    {
        return from_any_url(std ::move(spec));
    }

    Channel ChannelContext::from_url(specs::ChannelSpec&& spec)
    {
        auto platforms = make_platforms(spec.clear_platform_filters(), m_context.platforms());
        auto chan = from_any_url(std::move(spec));
        chan.m_platforms = std::move(platforms);
        return chan;
    }

    Channel ChannelContext::from_name(specs::ChannelSpec&& spec)
    {
        std::string name = spec.clear_location();
        const auto& custom_channels = get_custom_channels();
        const auto it_end = custom_channels.end();
        auto it = custom_channels.find(name);
        {
            std::string considered_name = name;
            while (it == it_end)
            {
                if (const auto pos = considered_name.rfind("/"); pos != std::string::npos)
                {
                    considered_name = considered_name.substr(0, pos);
                    it = custom_channels.find(considered_name);
                }
                else
                {
                    break;
                }
            }
        }

        if (it != it_end)
        {
            auto url = it->second.url();
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
                    url.append_path(name);
                    combined_name += "/" + name;
                }
                else
                {
                    // NOTE We assume that the `common_str`, if not empty, is necessarily at the
                    // beginning of `name` and at the end of `combined_name` (I don't know about
                    // other use cases for now)
                    combined_name += name.substr(common_str.size());
                    url.append_path(name.substr(common_str.size()));
                }
            }

            return Channel(
                /* url= */ std::move(url),
                /* location= */ it->second.location(),
                /* name= */ std::move(combined_name),
                /* canonical_name= */ std::move(name),
                /* platforms= */ make_platforms(spec.clear_platform_filters(), m_context.platforms())
            );
        }

        const auto& alias = get_channel_alias();
        auto url = alias;
        url.append_path(name);
        return Channel(
            /* url= */ std::move(url),
            /* location= */ alias.pretty_str(specs::CondaURL::StripScheme::yes, '/', specs::CondaURL::Credentials::Remove),
            /* name= */ name,
            /* canonical_name= */ name,
            /* platforms= */ make_platforms(spec.clear_platform_filters(), m_context.platforms())
        );
    }

    Channel ChannelContext::from_value(const std::string& in_value)
    {
        if (INVALID_CHANNELS.count(in_value) > 0)
        {
            return Channel(
                /* url= */ specs::CondaURL{},
                /* location= */ "",
                /* name= */ UNKNOWN_CHANNEL,
                /* canonical_name= */ UNKNOWN_CHANNEL
            );
        }

        auto spec = specs::ChannelSpec::parse(in_value);

        switch (spec.type())
        {
            case specs::ChannelSpec::Type::PackagePath:
            {
                return from_package_path(std::move(spec));
            }
            case specs::ChannelSpec::Type::Path:
            {
                return from_path(std::move(spec));
            }
            case specs::ChannelSpec::Type::PackageURL:
            {
                return from_package_url(std::move(spec));
            }
            case specs::ChannelSpec::Type::URL:
            {
                return from_url(std::move(spec));
            }
            case specs::ChannelSpec::Type::Name:
            {
                return from_name(std::move(spec));
            }
        }
        throw std::invalid_argument("Invalid ChannelSpec::Type");
    }

    const Channel& ChannelContext::make_channel(const std::string& value)
    {
        if (const auto it = m_channel_cache.find(value); it != m_channel_cache.end())
        {
            return it->second;
        }

        auto chan = from_value(value);

        set_fallback_credential_from_db(chan.m_url, m_context.authentication_info());

        auto [it, inserted] = m_channel_cache.emplace(value, std::move(chan));
        assert(inserted);
        return it->second;
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

    const specs::CondaURL& ChannelContext::get_channel_alias() const
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
        , m_channel_alias(specs::CondaURL::parse(util::path_or_url_to_url(m_context.channel_alias)))
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
