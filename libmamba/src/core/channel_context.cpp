// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <cassert>
#include <tuple>
#include <utility>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba
{

    /*********************************
     * ChannelContext implementation *
     *********************************/

    namespace
    {
        auto create_platforms(const std::vector<std::string>& platforms)
            -> specs::ChannelResolveParams::platform_list
        {
            return { platforms.cbegin(), platforms.cend() };
        }

        template <typename Param>
        auto make_unique_chan(std::string_view loc, const Param& params) -> specs::Channel
        {
            return specs::UnresolvedChannel::parse(loc)
                .and_then(  //
                    [&](specs::UnresolvedChannel&& uc)
                    { return specs::Channel::resolve(std::move(uc), params); }
                )
                .transform(
                    [](specs::Channel::channel_list&& channels) -> specs::Channel
                    {
                        assert(channels.size() == 1);
                        return std::move(channels.front());
                    }
                )
                .or_else([](specs::ParseError&& error) { throw std::move(error); })
                .value();
        };

        auto make_simple_params_base(const Context& ctx) -> specs::ChannelResolveParams
        {
            return specs::ChannelResolveParams{
                /* .platform= */ create_platforms(ctx.platforms()),
                /* .channel_alias= */
                specs::CondaURL::parse(util::path_or_url_to_url(ctx.channel_alias))
                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                    .value(),
                /* .custom_channels= */ {},
                /* .custom_multichannels= */ {},
                /* .authentication_db= */ ctx.authentication_info(),
                /* .home_dir= */ util::user_home_dir(),
                /* .current_working_dir= */ fs::current_path(),
            };
        }

        void add_simple_params_custom_channel(specs::ChannelResolveParams& params, const Context& ctx)
        {
            for (const auto& [name, location] : ctx.custom_channels)
            {
                auto chan = make_unique_chan(location, params);
                chan.set_display_name(name);
                params.custom_channels.emplace(name, std::move(chan));
            }
        }

        template <bool on_win>
        auto conda_custom_channels()
        {
            using namespace std::literals::string_view_literals;

            static constexpr std::size_t count = 3 + (on_win ? 1 : 0);
            auto channels = std::array<std::pair<std::string_view, std::string_view>, count>{
                std::pair{ "pkgs/main"sv, "https://repo.anaconda.com/pkgs/main"sv },
                std::pair{ "pkgs/r"sv, "https://repo.anaconda.com/pkgs/r"sv },
                std::pair{ "pkgs/pro"sv, "https://repo.anaconda.com/pkgs/pro"sv },
            };
            if constexpr (on_win)
            {
                channels[3] = std::pair{ "pkgs/msys2"sv, "https://repo.anaconda.com/pkgs/msys2"sv };
            }
            return channels;
        }

        void add_conda_params_custom_channel(specs::ChannelResolveParams& params, const Context& ctx)
        {
            for (const auto& [name, location] : ctx.custom_channels)
            {
                // In Conda, with custom_channel `name: "https://domain.net/"`, the URL must resolve
                // to "https://domain.net/name"
                auto conda_location = util::concat_dedup_splits(
                    util::rstrip(location, '/'),
                    util::lstrip(name, '/'),
                    '/'
                );
                auto chan = make_unique_chan(conda_location, params);
                chan.set_display_name(name);
                params.custom_channels.emplace(name, std::move(chan));
            }

            // Hard coded Anaconda channels names.
            // This will not redefine them if the user has already defined these keys.
            for (const auto& [name, location] : conda_custom_channels<util::on_win>())
            {
                auto chan = make_unique_chan(location, params);
                chan.set_display_name(std::string(name));
                params.custom_channels.emplace(name, std::move(chan));
            }
        }

        void
        add_simple_params_custom_multichannel(specs::ChannelResolveParams& params, const Context& ctx)
        {
            for (const auto& [multi_name, location_list] : ctx.custom_multichannels)
            {
                auto channels = specs::ChannelResolveParams::channel_list();
                channels.reserve(location_list.size());
                for (auto& location : location_list)
                {
                    channels.push_back(make_unique_chan(location, params));
                }
                params.custom_multichannels.emplace(multi_name, std::move(channels));
            }
        }

        void
        add_conda_params_custom_multichannel(specs::ChannelResolveParams& params, const Context& ctx)
        {
            // Hard coded Anaconda "defaults" multi channel name.
            // This will not redefine them if the user has already defined these keys.
            if (auto it = ctx.custom_multichannels.find("defaults");
                it == ctx.custom_multichannels.cend())
            {
                auto channels = specs::ChannelResolveParams::channel_list();
                channels.reserve(ctx.default_channels.size());
                std::transform(
                    ctx.default_channels.cbegin(),
                    ctx.default_channels.cend(),
                    std::back_inserter(channels),
                    [&params](const auto& loc) { return make_unique_chan(loc, params); }
                );
                params.custom_multichannels.emplace("defaults", std::move(channels));
            }

            // Hard coded Anaconda "local" multi channel name.
            // This will not redefine them if the user has already defined these keys.
            if (auto it = ctx.custom_multichannels.find("local");
                it == ctx.custom_multichannels.cend())
            {
                auto channels = specs::ChannelResolveParams::channel_list();
                channels.reserve(3);
                for (auto path : {
                         ctx.prefix_params.target_prefix / "conda-bld",
                         ctx.prefix_params.root_prefix / "conda-bld",
                         fs::u8path(params.home_dir) / "conda-bld",
                     })
                {
                    if (fs::exists(path))
                    {
                        channels.push_back(make_unique_chan(path.string(), params));
                    }
                }
                params.custom_multichannels.emplace("local", std::move(channels));
            }

            // Called after to guarantee there are no custom multichannels when calling
            // make_unique_chan.
            add_simple_params_custom_multichannel(params, ctx);
        }

        auto create_zstd(const Context& ctx, specs::ChannelResolveParams params)
            -> std::vector<specs::Channel>
        {
            auto out = std::vector<specs::Channel>();
            if (ctx.repodata_use_zst)
            {
                out.reserve(ctx.repodata_has_zst.size());
                for (const auto& loc : ctx.repodata_has_zst)
                {
                    auto channels = specs::UnresolvedChannel::parse(loc)
                                        .and_then(  //
                                            [&](specs::UnresolvedChannel&& uc)
                                            {
                                                return specs::Channel::resolve(std::move(uc), params);
                                            }
                                        )
                                        .or_else([](specs::ParseError&& error)
                                                 { throw std::move(error); })
                                        .value();
                    for (auto& chan : channels)
                    {
                        out.push_back(std::move(chan));
                    }
                }
            }
            return out;
        }
    }

    ChannelContext::ChannelContext(ChannelResolveParams params, std::vector<Channel> has_zst)
        : m_channel_params(std::move(params))
        , m_has_zst(std::move(has_zst))
    {
    }

    auto ChannelContext::make_simple(const Context& ctx) -> ChannelContext
    {
        auto params = make_simple_params_base(ctx);
        add_simple_params_custom_channel(params, ctx);
        add_simple_params_custom_multichannel(params, ctx);
        auto has_zst = create_zstd(ctx, params);
        return { std::move(params), std::move(has_zst) };
    }

    auto ChannelContext::make_conda_compatible(const Context& ctx) -> ChannelContext
    {
        auto params = make_simple_params_base(ctx);
        add_conda_params_custom_channel(params, ctx);
        add_conda_params_custom_multichannel(params, ctx);
        auto has_zst = create_zstd(ctx, params);
        return { std::move(params), has_zst };
    }

    auto ChannelContext::make_channel(specs::UnresolvedChannel uc) -> const channel_list&
    {
        auto str = uc.str();
        if (const auto it = m_channel_cache.find(str); it != m_channel_cache.end())
        {
            return it->second;
        }

        auto [it, inserted] = m_channel_cache.emplace(
            std::move(str),
            Channel::resolve(std::move(uc), params())
                .or_else([](specs::ParseError&& err) { throw std::move(err); })
                .value()
        );
        assert(inserted);
        return it->second;
    }

    auto ChannelContext::make_channel(std::string_view name) -> const channel_list&
    {
        if (const auto it = m_channel_cache.find(std::string(name)); it != m_channel_cache.end())
        {
            return it->second;
        }

        auto [it, inserted] = m_channel_cache.emplace(
            name,
            specs::UnresolvedChannel::parse(name)
                .and_then(  //
                    [&](specs::UnresolvedChannel&& uc)
                    { return specs::Channel::resolve(std::move(uc), params()); }
                )
                .or_else([](specs::ParseError&& error) { throw std::move(error); })
                .value()
        );
        assert(inserted);
        return it->second;
    }

    auto ChannelContext::make_channel(std::string_view name, const std::vector<std::string>& mirrors)
        -> const channel_list&
    {
        if (const auto it = m_channel_cache.find(std::string(name)); it != m_channel_cache.end())
        {
            return it->second;
        }

        std::vector<specs::CondaURL> mirror_urls;
        mirror_urls.reserve(mirrors.size());
        for (const auto& mirror : mirrors)
        {
            mirror_urls.push_back(  //
                specs::CondaURL::parse(mirror)
                    .or_else([](specs::ParseError&& err) { throw std::move(err); })
                    .value()
            );
        }
        auto [it, inserted] = m_channel_cache.emplace(
            name,
            channel_list{
                Channel(std::move(mirror_urls), std::string(name), m_channel_params.platforms) }
        );
        assert(inserted);
        return it->second;
    }

    auto ChannelContext::params() const -> const specs::ChannelResolveParams&
    {
        return m_channel_params;
    }

    auto ChannelContext::has_zst(const Channel& chan) const -> bool
    {
        for (const auto& zst_chan : m_has_zst)
        {
            if (zst_chan.contains_equivalent(chan))
            {
                return true;
            }
        }
        return false;
    }
}
