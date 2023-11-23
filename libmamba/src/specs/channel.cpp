// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <stdexcept>

#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/channel.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/tuple_hash.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::specs
{
    /*********************************
     *  NameWeakener Implementation  *
     *********************************/

    auto ChannelResolveParams::NameWeakener::make_first_key(std::string_view key) const
        -> std::string_view
    {
        return key;
    }

    auto ChannelResolveParams::NameWeakener::weaken_key(std::string_view key) const
        -> std::optional<std::string_view>
    {
        return std::get<0>(util::rsplit_once(key, '/'));
    }

    /*******************************
     *  Implementation of Channel  *
     *******************************/

    Channel::Channel(CondaURL url, std::string display_name, util::flat_set<std::string> platforms)
        : m_url(std::move(url))
        , m_display_name(std::move(display_name))
        , m_platforms(std::move(platforms))
    {
        auto p = m_url.clear_path();
        p = util::rstrip(p, '/');
        m_url.set_path(std::move(p));
    }

    auto Channel::is_package() const -> bool
    {
        return !url().package().empty();
    }

    auto Channel::url() const -> const CondaURL&
    {
        return m_url;
    }

    auto Channel::clear_url() -> const CondaURL
    {
        return std::exchange(m_url, {});
    }

    void Channel::set_url(CondaURL url)
    {
        m_url = std::move(url);
    }

    auto Channel::platform_urls() const -> std::vector<CondaURL>
    {
        if (is_package())
        {
            return { url() };
        }

        auto out = std::vector<CondaURL>();
        out.reserve(platforms().size());
        for (const auto& platform : platforms())
        {
            out.push_back(platform_url(platform));
        }
        return out;
    }

    auto Channel::platform_url(std::string_view platform) const -> CondaURL
    {
        if (is_package())
        {
            return url();
        }
        return (url() / platform);
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
        using Decode = typename CondaURL::Decode;

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

    /****************************************
     *  Implementation of Channel::resolve  *
     ****************************************/

    namespace
    {
        auto url_match(const CondaURL& registered, const CondaURL& candidate) -> bool
        {
            using Decode = typename CondaURL::Decode;

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

        void set_fallback_credential_from_auth(CondaURL& url, const AuthenticationInfo& auth)
        {
            std::visit(
                [&](const auto& info)
                {
                    using Info = std::decay_t<decltype(info)>;
                    if constexpr (std::is_same_v<Info, BasicHTTPAuthentication>)
                    {
                        if (!url.has_user() && !url.has_password())
                        {
                            url.set_user(info.user, CondaURL::Encode::yes);
                            url.set_password(info.password, CondaURL::Encode::yes);
                        }
                    }
                    else if constexpr (std::is_same_v<Info, CondaToken>)
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

        void set_fallback_credential_from_db(CondaURL& url, const AuthenticationDataBase& db)
        {
            if (!url.has_token() || !url.has_user() || !url.has_password())
            {
                const auto key = url.pretty_str(
                    CondaURL::StripScheme::yes,
                    '/',
                    CondaURL::Credentials::Remove
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

        auto resolve_path_name(const CondaURL& uri, ChannelResolveParamsView params) -> std::string
        {
            for (const auto& [display_name, chan] : params.custom_channels)
            {
                if (url_match(chan.url(), uri))
                {
                    return display_name;
                }
            }

            if (const auto& ca = params.channel_alias; url_match(ca, uri))
            {
                return std::string(util::strip(util::remove_prefix(uri.path(), ca.path()), '/'));
            }

            return uri.pretty_str();
        }

        auto resolve_path_location(std::string location, ChannelResolveParamsView params)
            -> std::string
        {
            if (util::url_has_scheme(location))
            {
                return location;
            }

            const auto home = util::path_to_posix(std::string(params.home_dir));
            auto path = fs::u8path(util::expand_home(location, home));
            if (path.is_relative())
            {
                path = (fs::u8path(params.current_working_dir) / std::move(path)).lexically_normal();
            }
            return util::abs_path_to_url(std::move(path).string());
        }

        auto resolve_path(ChannelSpec&& spec, ChannelResolveParamsView params) -> Channel
        {
            auto uri = CondaURL::parse(resolve_path_location(spec.clear_location(), params));
            auto display_name = resolve_path_name(uri, params);
            auto platforms = ChannelResolveParams::platform_list{};
            if (spec.type() == ChannelSpec::Type::Path)
            {
                platforms = make_platforms(spec.clear_platform_filters(), params.platforms);
            }

            return { std::move(uri), std::move(display_name), std::move(platforms) };
        }

        auto resolve_url_name(const CondaURL& url, ChannelResolveParamsView params) -> std::string
        {
            using StripScheme = typename CondaURL::StripScheme;
            using Credentials = typename CondaURL::Credentials;

            std::string url_str = url.pretty_str(StripScheme::yes, '/', Credentials::Remove);

            for (const auto& [display_name, chan] : params.custom_channels)
            {
                if (url_match(chan.url(), url))
                {
                    return display_name;
                }
            }

            if (const auto& ca = params.channel_alias; url_match(ca, url))
            {
                auto ca_str = ca.pretty_str(StripScheme::yes, '/', Credentials::Remove);
                return std::string(util::strip(util::remove_prefix(url_str, ca_str), '/'));
            }

            return url.pretty_str(StripScheme::no, '/', Credentials::Remove);
        }

        auto resolve_url(ChannelSpec&& spec, ChannelResolveParamsView params) -> Channel
        {
            assert(util::url_has_scheme(spec.location()));
            assert(util::url_get_scheme(spec.location()) != "file");

            auto url = CondaURL::parse(spec.location());
            auto display_name = resolve_url_name(url, params);
            set_fallback_credential_from_db(url, params.authentication_db);
            auto platforms = ChannelResolveParams::platform_list{};
            if (spec.type() == ChannelSpec::Type::URL)
            {
                platforms = make_platforms(spec.clear_platform_filters(), params.platforms);
            }

            return { std::move(url), std::move(display_name), std::move(platforms) };
        }

        auto resolve_name_in_custom_channel(
            ChannelSpec&& spec,
            ChannelResolveParamsView params,
            std::string_view match_name,
            const Channel& match_chan

        ) -> Channel
        {
            auto url = match_chan.url();
            url.append_path(util::remove_prefix(spec.location(), match_name));
            set_fallback_credential_from_db(url, params.authentication_db);
            return {
                /* url= */ std::move(url),
                /* display_name= */ spec.clear_location(),
                /* platforms= */ make_platforms(spec.clear_platform_filters(), params.platforms),
            };
        }

        auto resolve_name_in_custom_mulitchannels(
            const ChannelSpec& spec,
            ChannelResolveParamsView params,
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

        auto resolve_name_from_alias(ChannelSpec&& spec, ChannelResolveParamsView params) -> Channel
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

        auto resolve_name(ChannelSpec&& spec, ChannelResolveParamsView params) -> Channel::channel_list
        {
            if (auto it = params.custom_channels.find_weaken(spec.location());
                it != params.custom_channels.cend())
            {
                return {
                    resolve_name_in_custom_channel(std::move(spec), params, it->first, it->second)
                };
            }

            if (auto it = params.custom_multichannels.find(spec.location());
                it != params.custom_multichannels.end())
            {
                return resolve_name_in_custom_mulitchannels(spec, params, it->second);
            }

            return { resolve_name_from_alias(std::move(spec), params) };
        }
    }


    auto Channel::resolve(ChannelSpec spec, ChannelResolveParamsView params) -> channel_list
    {
        switch (spec.type())
        {
            case ChannelSpec::Type::PackagePath:
            case ChannelSpec::Type::Path:
            {
                return { resolve_path(std::move(spec), params) };
            }
            case ChannelSpec::Type::PackageURL:
            case ChannelSpec::Type::URL:
            {
                return { resolve_url(std::move(spec), params) };
            }
            case ChannelSpec::Type::Name:
            {
                return resolve_name(std::move(spec), params);
            }
            case ChannelSpec::Type::Unknown:
            {
                return { { CondaURL{}, spec.clear_location() } };
            }
        }
        throw std::invalid_argument("Invalid ChannelSpec::Type");
    }

    auto Channel::resolve(ChannelSpec spec, const ChannelResolveParams& params) -> channel_list
    {
        return resolve(
            std::move(spec),
            ChannelResolveParamsView{
                /* .platforms= */ params.platforms,
                /* .channel_alias= */ params.channel_alias,
                /* .custom_channels= */ params.custom_channels,
                /* .custom_multichannels= */ params.custom_multichannels,
                /* .authentication_db= */ params.authentication_db,
                /* .home_dir= */ params.home_dir,
                /* .current_working_dir= */ params.current_working_dir,
            }
        );
    }

    /**********************************
     *  Implementation of comparison  *
     **********************************/

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

/*********************************
 *  Implementation of std::hash  *
 *********************************/

auto ::std::hash<mamba::specs::Channel>::operator()(const mamba::specs::Channel& chan) const
    -> std::size_t
{
    return mamba::util::hash_combine(
        mamba::util::hash_vals(chan.url(), chan.display_name()),
        mamba::util::hash_range(chan.platforms())
    );
}
