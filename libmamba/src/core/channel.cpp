// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <set>
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
    namespace
    {
        const char UNKNOWN_CHANNEL[] = "<unknown>";

        const std::set<std::string> INVALID_CHANNELS = { "<unknown>",
                                                         "None:///<unknown>",
                                                         "None",
                                                         "",
                                                         ":///<unknown>" };
    }

    std::vector<std::string> get_known_platforms()
    {
        auto plats = specs::known_platform_names();
        return { plats.begin(), plats.end() };
    }

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

        for (const auto& [canonical_name, chan] : get_custom_channels())
        {
            if (url_match(chan.url(), uri))
            {
                return Channel(
                    /* url= */ std::move(uri),
                    /* canonical_name= */ std::string(canonical_name)
                );
            }
        }

        if (const auto& ca = get_channel_alias(); url_match(ca, uri))
        {
            auto name = util::strip(util::remove_prefix(uri.path(), ca.path()), '/');
            return Channel(
                /* url= */ std::move(uri),
                /* canonical_name= */ std::string(name)
            );
        }

        auto canonical_name = uri.pretty_str();
        return Channel(
            /* url= */ std::move(uri),
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

        std::string url_str = url.pretty_str(StripScheme::yes, '/', Credentials::Remove);

        for (const auto& [canonical_name, chan] : get_custom_channels())
        {
            if (url_match(chan.url(), url))
            {
                return Channel(
                    /* url= */ std::move(url),
                    /* canonical_name= */ std::string(canonical_name)
                );
            }
        }

        if (const auto& ca = get_channel_alias(); url_match(ca, url))
        {
            auto ca_str = ca.pretty_str(StripScheme::yes, '/', Credentials::Remove);
            auto name = std::string(util::strip(util::remove_prefix(url_str, ca_str), '/'));
            return Channel(
                /* url= */ std::move(url),
                /* canonical_name= */ name
            );
        }

        auto canonical_name = url.pretty_str(StripScheme::no, '/', Credentials::Remove);
        return Channel(
            /* url= */ std::move(url),
            /* canonical_name= */ std::move(canonical_name)
        );
    }

    Channel ChannelContext::from_package_url(specs::ChannelSpec&& spec)
    {
        auto chan = from_any_url(std ::move(spec));
        set_fallback_credential_from_db(chan.m_url, m_context.authentication_info());
        return chan;
    }

    Channel ChannelContext::from_url(specs::ChannelSpec&& spec)
    {
        auto platforms = make_platforms(spec.clear_platform_filters(), m_context.platforms());
        auto chan = from_any_url(std::move(spec));
        chan.m_platforms = std::move(platforms);
        set_fallback_credential_from_db(chan.m_url, m_context.authentication_info());
        return chan;
    }

    Channel ChannelContext::from_name(specs::ChannelSpec&& spec)
    {
        std::string name = spec.clear_location();
        const auto& custom_channels = get_custom_channels();
        const auto it_end = custom_channels.end();
        auto it = it_end;
        {
            auto considered_name = std::optional<std::string_view>(name);
            while ((it == it_end))
            {
                if (!considered_name.has_value())
                {
                    break;
                }
                it = custom_channels.find(std::string(considered_name.value()));
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

            set_fallback_credential_from_db(url, m_context.authentication_info());
            return Channel(
                /* url= */ std::move(url),
                /* canonical_name= */ std::move(name),
                /* platforms= */ make_platforms(spec.clear_platform_filters(), m_context.platforms())
            );
        }

        const auto& alias = get_channel_alias();
        auto url = alias;
        url.append_path(name);
        set_fallback_credential_from_db(url, m_context.authentication_info());
        return Channel(
            /* url= */ std::move(url),
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
