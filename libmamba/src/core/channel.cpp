// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <tuple>
#include <utility>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/tuple_hash.hpp"
#include "mamba/util/url.hpp"
#include "mamba/util/url_manip.hpp"


namespace mamba
{
    /*********************************
     *  NameWeakener Implementation  *
     *********************************/

    auto Channel::ResolveParams::NameWeakener::make_first_key(std::string_view key) const
        -> std::string_view
    {
        return key;
    }

    auto Channel::ResolveParams::NameWeakener::weaken_key(std::string_view key) const
        -> std::optional<std::string_view>
    {
        return std::get<0>(util::rsplit_once(key, '/'));
    }

    /**************************
     * Channel implementation *
     **************************/

    Channel::Channel(specs::CondaURL url, std::string display_name, util::flat_set<std::string> platforms)
        : m_url(std::move(url))
        , m_display_name(std::move(display_name))
        , m_platforms(std::move(platforms))
    {
        auto p = m_url.clear_path();
        p = util::rstrip(p, '/');
        m_url.set_path(std::move(p));
    }

    auto Channel::url() const -> const specs::CondaURL&
    {
        return m_url;
    }

    auto Channel::clear_url() -> const specs::CondaURL
    {
        return std::exchange(m_url, {});
    }

    void Channel::set_url(specs::CondaURL url)
    {
        m_url = std::move(url);
    }

    auto Channel::platforms() const -> const platform_list&
    {
        return m_platforms;
    }

    auto Channel::clear_platforms() -> platform_list
    {
        return std::exchange(m_platforms, {});
    }

    void Channel::set_platforms(platform_list platforms)
    {
        m_platforms = std::move(platforms);
    }

    auto Channel::display_name() const -> const std::string&
    {
        return m_display_name;
    }

    auto Channel::clear_display_name() -> std::string
    {
        return std::exchange(m_display_name, {});
    }

    void Channel::set_display_name(std::string display_name)
    {
        m_display_name = std::move(display_name);
    }

    auto Channel::url_equivalent_with(const Channel& other) const -> bool
    {
        using Decode = typename specs::CondaURL::Decode;

        const auto& this_url = url();
        const auto& other_url = other.url();
        // Not checking users, passwords, and tokens
        return
            // Schemes
            (this_url.scheme() == other_url.scheme())
            // Hosts
            && (this_url.host(Decode::no) == other_url.host(Decode::no))
            // Different ports are considered different channels
            && (this_url.port() == other_url.port())
            // Removing potential trailing '/'
            && (util::rstrip(this_url.path_without_token(Decode::no), '/')
                == util::rstrip(other_url.path_without_token(Decode::no), '/'));
    }

    auto Channel::contains_equivalent(const Channel& other) const -> bool
    {
        return url_equivalent_with(other) && util::set_is_superset_of(platforms(), other.platforms());
    }

    auto Channel::urls(bool with_credential) const -> util::flat_set<std::string>
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

    auto Channel::platform_urls(bool with_credential) const
        -> util::flat_set<std::pair<std::string, std::string>>
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

    auto Channel::platform_url(std::string_view platform, bool with_credential) const -> std::string
    {
        auto cred = with_credential ? specs::CondaURL::Credentials::Show
                                    : specs::CondaURL::Credentials::Remove;

        if (!url().package().empty())
        {
            return url().str(cred);
        }
        return (url() / platform).str(cred);
    }

    namespace
    {
        auto attrs(const Channel& chan)
        {
            return std::tie(chan.url(), chan.platforms(), chan.display_name());
        }
    }

    /** Tuple-like equality of all observable members */
    auto operator==(const Channel& a, const Channel& b) -> bool
    {
        return attrs(a) == attrs(b);
    }

    auto operator!=(const Channel& a, const Channel& b) -> bool
    {
        return !(a == b);
    }
}

auto
std::hash<mamba::Channel>::operator()(const mamba::Channel& chan) const -> std::size_t
{
    return mamba::util::hash_combine(
        mamba::util::hash_vals(chan.url(), chan.display_name()),
        mamba::util::hash_range(chan.platforms())
    );
}

namespace mamba
{

    /*********************************
     * ChannelContext implementation *
     *********************************/

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
                if (auto it = db.find_weaken(key); it != db.end())
                {
                    set_fallback_credential_from_auth(url, it->second);
                }
            }
        }

        auto
        make_platforms(util::flat_set<std::string> filters, const util::flat_set<std::string>& defaults)
        {
            return filters.empty() ? defaults : filters;
        }

        auto resolve_path_name(const specs::CondaURL& uri, Channel::ResolveParams params)
            -> std::string
        {
            for (const auto& [display_name, chan] : params.custom_channels)
            {
                if (url_match(chan.url(), uri))
                {
                    return std::string(display_name);
                }
            }

            if (const auto& ca = params.channel_alias; url_match(ca, uri))
            {
                return std::string(util::strip(util::remove_prefix(uri.path(), ca.path()), '/'));
            }

            return uri.pretty_str();
        }

        auto resolve_path_location(std::string location, Channel::ResolveParams params) -> std::string
        {
            if (util::url_has_scheme(location))
            {
                return location;
            }

            const auto home = util::path_to_posix(std::string(params.home_dir));
            auto path = fs::u8path(util::expand_home(location, home));
            if (path.is_relative())
            {
                path = fs::proximate(path, params.current_working_dir);
            }
            return util::abs_path_to_url(std::move(path).lexically_normal().string());
        }

        auto resolve_path(specs::ChannelSpec&& spec, Channel::ResolveParams params) -> Channel
        {
            auto uri = specs::CondaURL::parse(resolve_path_location(spec.clear_location(), params));
            auto display_name = resolve_path_name(uri, params);
            auto platforms = Channel::ResolveParams::platform_list{};
            if (spec.type() == specs::ChannelSpec::Type::Path)
            {
                platforms = make_platforms(spec.clear_platform_filters(), params.platforms);
            }

            return { std::move(uri), std::move(display_name), std::move(platforms) };
        }

        auto resolve_url_name(const specs::CondaURL& url, Channel::ResolveParams params) -> std::string
        {
            using StripScheme = typename specs::CondaURL::StripScheme;
            using Credentials = typename specs::CondaURL::Credentials;

            std::string url_str = url.pretty_str(StripScheme::yes, '/', Credentials::Remove);

            for (const auto& [display_name, chan] : params.custom_channels)
            {
                if (url_match(chan.url(), url))
                {
                    return std::string(display_name);
                }
            }

            if (const auto& ca = params.channel_alias; url_match(ca, url))
            {
                auto ca_str = ca.pretty_str(StripScheme::yes, '/', Credentials::Remove);
                return std::string(util::strip(util::remove_prefix(url_str, ca_str), '/'));
            }

            return url.pretty_str(StripScheme::no, '/', Credentials::Remove);
        }

        auto resolve_url(specs::ChannelSpec&& spec, Channel::ResolveParams params) -> Channel
        {
            assert(util::url_has_scheme(spec.location()));
            assert(util::url_get_scheme(spec.location()) != "file");

            auto url = specs::CondaURL::parse(spec.location());
            auto display_name = resolve_url_name(url, params);
            set_fallback_credential_from_db(url, params.authentication_db);
            auto platforms = Channel::ResolveParams::platform_list{};
            if (spec.type() == specs::ChannelSpec::Type::URL)
            {
                platforms = make_platforms(spec.clear_platform_filters(), params.platforms);
            }

            return { std::move(url), std::move(display_name), std::move(platforms) };
        }

        auto resolve_name_in_custom_channel(
            specs::ChannelSpec&& spec,
            Channel::ResolveParams params,
            const Channel& match
        ) -> Channel
        {
            auto url = match.url();
            // we can have a channel like
            // testchannel: https://server.com/private/testchannel
            // where `name == private/testchannel` and we need to join the remaining label part
            // of the channel (e.g. -c testchannel/mylabel/xyz)
            // needs to result in `name = private/testchannel/mylabel/xyz`
            std::string combined_name = util::concat_dedup_splits(
                util::rstrip(url.path(), '/'),
                util::lstrip(spec.location(), '/'),
                '/'
            );
            url.set_path(combined_name);

            set_fallback_credential_from_db(url, params.authentication_db);
            return {
                /* url= */ std::move(url),
                /* display_name= */ spec.clear_location(),
                /* platforms= */ make_platforms(spec.clear_platform_filters(), params.platforms),
            };
        }

        auto resolve_name_in_custom_mulitchannels(
            const specs::ChannelSpec& spec,
            Channel::ResolveParams params,
            const Channel::channel_list& matches
        ) -> Channel::channel_list
        {
            auto out = Channel::channel_list();
            out.reserve(matches.size());

            for (const auto& chan : matches)
            {
                auto url = chan.url();
                set_fallback_credential_from_db(url, params.authentication_db);
                out.emplace_back(
                    /* url= */ std::move(url),
                    /* display_name= */ chan.display_name(),  // Not using multi_channel name
                    /* platforms= */ make_platforms(spec.platform_filters(), params.platforms)
                );
            }
            return out;
        }

        auto resolve_name_from_alias(specs::ChannelSpec&& spec, Channel::ResolveParams params)
            -> Channel
        {
            auto url = params.channel_alias;
            url.append_path(spec.location());
            set_fallback_credential_from_db(url, params.authentication_db);
            return {
                /* url= */ std::move(url),
                /* display_name= */ spec.clear_location(),
                /* platforms= */ make_platforms(spec.clear_platform_filters(), params.platforms),
            };
        }

        auto resolve_name(specs::ChannelSpec&& spec, Channel::ResolveParams params)
            -> Channel::channel_list
        {
            if (auto it = params.custom_channels.find_weaken(spec.location());
                it != params.custom_channels.cend())
            {
                return { resolve_name_in_custom_channel(std::move(spec), params, it->second) };
            }

            if (auto it = params.custom_multichannels.find(spec.location());
                it != params.custom_multichannels.end())
            {
                return resolve_name_in_custom_mulitchannels(spec, params, it->second);
            }

            return { resolve_name_from_alias(std::move(spec), params) };
        }
    }

    auto Channel::resolve(specs::ChannelSpec spec, ResolveParams params) -> channel_list
    {
        switch (spec.type())
        {
            case specs::ChannelSpec::Type::PackagePath:
            case specs::ChannelSpec::Type::Path:
            {
                return { resolve_path(std::move(spec), params) };
            }
            case specs::ChannelSpec::Type::PackageURL:
            case specs::ChannelSpec::Type::URL:
            {
                return { resolve_url(std::move(spec), params) };
            }
            case specs::ChannelSpec::Type::Name:
            {
                return resolve_name(std::move(spec), params);
            }
            case specs::ChannelSpec::Type::Unknown:
            {
                return { { specs::CondaURL{}, spec.clear_location() } };
            }
        }
        throw std::invalid_argument("Invalid ChannelSpec::Type");
    }

    auto ChannelContext::params() -> Channel::ResolveParams
    {
        return {
            /* .platforms= */ m_platforms,
            /* .channel_alias= */ m_channel_alias,
            /* .custom_channels= */ m_custom_channels,
            /* .custom_multichannels= */ m_custom_multichannels,
            /* .authentication_db= */ m_context.authentication_info(),
            /* .home_dir= */ m_home_dir,
            /* .current_working_dir= */ m_current_working_dir,
        };
    }

    auto ChannelContext::make_channel(std::string_view name) -> channel_list
    {
        if (const auto it = m_channel_cache.find(std::string(name)); it != m_channel_cache.end())
        {
            return it->second;
        }

        auto [it, inserted] = m_channel_cache.emplace(
            name,
            Channel::resolve(specs::ChannelSpec::parse(name), params())
        );
        assert(inserted);
        return { it->second };
    }

    auto ChannelContext::get_channel_alias() const -> const specs::CondaURL&
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
        , m_home_dir(util::user_home_dir())
        , m_current_working_dir(fs::current_path())
    {
        {
            const auto& plats = m_context.platforms();
            m_platforms = Channel::ResolveParams::platform_list(plats.cbegin(), plats.cend());
        }
        init_custom_channels();
    }

    ChannelContext::~ChannelContext() = default;

    void ChannelContext::init_custom_channels()
    {
        for (const auto& [name, location] : m_context.custom_channels)
        {
            auto channels = make_channel(location);
            assert(channels.size() == 1);
            channels.front().set_display_name(name);
            m_custom_channels.emplace(name, std::move(channels.front()));
        }

        for (const auto& [multi_name, location_list] : m_context.custom_multichannels)
        {
            auto channels = channel_list();
            channels.reserve(location_list.size());
            for (auto& location : location_list)
            {
                for (auto& chan : make_channel(location))
                {
                    channels.push_back(std::move(chan));
                }
            }
            m_custom_multichannels.emplace(multi_name, std::move(channels));
        }
    }
}  // namespace mamba
