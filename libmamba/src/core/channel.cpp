// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "mamba/core/channel.hpp"
#include "mamba/core/context.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/tuple_hash.hpp"
#include "mamba/util/url.hpp"
#include "mamba/util/url_manip.hpp"


namespace mamba
{
    /**************************
     * Channel implementation *
     **************************/

    Channel::Channel(specs::CondaURL url, std::string canonical_name, util::flat_set<std::string> platforms)
        : m_url(std::move(url))
        , m_canonical_name(std::move(canonical_name))
        , m_platforms(std::move(platforms))
    {
    }

    const specs::CondaURL& Channel::url() const
    {
        return m_url;
    }

    const util::flat_set<std::string>& Channel::platforms() const
    {
        return m_platforms;
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

    namespace
    {
        auto attrs(const Channel& chan)
        {
            return std::tie(chan.url(), chan.platforms(), chan.canonical_name());
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
        mamba::util::hash_vals(chan.url(), chan.canonical_name()),
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
                if (auto it = db.find_compatible(key); it != db.end())
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
            for (const auto& [canonical_name, chan] : params.custom_channels)
            {
                if (url_match(chan.url(), uri))
                {
                    return std::string(canonical_name);
                }
            }

            if (const auto& ca = params.channel_alias; url_match(ca, uri))
            {
                return std::string(util::strip(util::remove_prefix(uri.path(), ca.path()), '/'));
            }

            return uri.pretty_str();
        }

        auto resolve_path(specs::ChannelSpec&& spec, Channel::ResolveParams params) -> Channel
        {
            auto uri = specs::CondaURL::parse(util::path_or_url_to_url(spec.location()));
            auto canonical_name = resolve_path_name(uri, params);
            auto platforms = Channel::ResolveParams::platform_list{};
            if (spec.type() == specs::ChannelSpec::Type::Path)
            {
                platforms = make_platforms(spec.clear_platform_filters(), params.platforms);
            }

            return Channel(std::move(uri), std::move(canonical_name), std::move(platforms));
        }

        auto resolve_url_name(const specs::CondaURL& url, Channel::ResolveParams params) -> std::string
        {
            using StripScheme = typename specs::CondaURL::StripScheme;
            using Credentials = typename specs::CondaURL::Credentials;

            std::string url_str = url.pretty_str(StripScheme::yes, '/', Credentials::Remove);

            for (const auto& [canonical_name, chan] : params.custom_channels)
            {
                if (url_match(chan.url(), url))
                {
                    return std::string(canonical_name);
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
            auto canonical_name = resolve_url_name(url, params);
            set_fallback_credential_from_db(url, params.auth_db);
            auto platforms = Channel::ResolveParams::platform_list{};
            if (spec.type() == specs::ChannelSpec::Type::URL)
            {
                platforms = make_platforms(spec.clear_platform_filters(), params.platforms);
            }

            return Channel(std::move(url), std::move(canonical_name), std::move(platforms));
        }

        auto resolve_name(specs::ChannelSpec&& spec, Channel::ResolveParams params) -> Channel
        {
            std::string name = spec.clear_location();
            const auto it_end = params.custom_channels.end();
            auto it = it_end;
            {
                auto considered_name = std::optional<std::string_view>(name);
                while ((it == it_end))
                {
                    if (!considered_name.has_value())
                    {
                        break;
                    }
                    it = params.custom_channels.find(std::string(considered_name.value()));
                    considered_name = std::get<0>(util::rsplit_once(considered_name.value(), '/'));
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
                std::string combined_name = util::concat_dedup_splits(
                    util::rstrip(url.path(), '/'),
                    util::lstrip(name, '/'),
                    '/'
                );
                url.set_path(combined_name);

                set_fallback_credential_from_db(url, params.auth_db);
                return Channel(
                    /* url= */ std::move(url),
                    /* canonical_name= */ std::move(name),
                    /* platforms= */ make_platforms(spec.clear_platform_filters(), params.platforms)
                );
            }

            auto url = params.channel_alias;
            url.append_path(name);
            set_fallback_credential_from_db(url, params.auth_db);
            return Channel(
                /* url= */ std::move(url),
                /* canonical_name= */ name,
                /* platforms= */ make_platforms(spec.clear_platform_filters(), params.platforms)
            );
        }
    }

    Channel ChannelContext::from_value(const std::string& in_value)
    {
        auto spec = specs::ChannelSpec::parse(in_value);
        const auto platforms = [](const auto& plats) {
            return Channel::ResolveParams::platform_list(plats.cbegin(), plats.cend());
        }(m_context.platforms());
        auto params = Channel::ResolveParams{
            /* .platforms */ platforms,
            /* .channel_alias */ m_channel_alias,
            /* .custom_channels */ m_custom_channels,
            /* .auth_db */ m_context.authentication_info(),
        };

        switch (spec.type())
        {
            case specs::ChannelSpec::Type::PackagePath:
            case specs::ChannelSpec::Type::Path:
            {
                return resolve_path(std::move(spec), params);
            }
            case specs::ChannelSpec::Type::PackageURL:
            case specs::ChannelSpec::Type::URL:
            {
                return resolve_url(std::move(spec), params);
            }
            case specs::ChannelSpec::Type::Name:
            {
                return resolve_name(std::move(spec), params);
            }
            case specs::ChannelSpec::Type::Unknown:
            {
                return Channel(specs::CondaURL{}, spec.clear_location());
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

        auto [it, inserted] = m_channel_cache.emplace(value, from_value(value));
        assert(inserted);
        return it->second;
    }

    auto ChannelContext::get_channels(const std::vector<std::string>& channel_names) -> channel_list
    {
        auto added = std::unordered_set<Channel>();
        auto result = channel_list();
        for (auto name : channel_names)
        {
            auto spec = specs::ChannelSpec::parse(name);

            const auto& multi_chan = get_custom_multichannels();
            if (auto iter = multi_chan.find(spec.location()); iter != multi_chan.end())
            {
                for (const auto& chan : iter->second)
                {
                    auto channel = chan;
                    if (!spec.platform_filters().empty())
                    {
                        channel.m_platforms = spec.platform_filters();
                    }
                    if (added.insert(channel).second)
                    {
                        result.push_back(std::move(channel));
                    }
                }
            }
            else
            {
                auto channel = make_channel(name);
                if (added.insert(channel).second)
                {
                    result.push_back(std::move(channel));
                }
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
        for (const auto& [name, location] : m_context.custom_channels)
        {
            auto channel = from_value(location);
            channel.m_canonical_name = name;
            m_custom_channels.emplace(name, std::move(channel));
        }

        for (const auto& [multi_name, location_list] : m_context.custom_multichannels)
        {
            auto channels = channel_list();
            channels.reserve(location_list.size());
            for (auto& location : location_list)
            {
                channels.push_back(from_value(location));
            }
            m_custom_multichannels.emplace(multi_name, std::move(channels));
        }
    }
}  // namespace mamba
