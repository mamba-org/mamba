// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>
#include <tuple>
#include <utility>

#include "mamba/core/channel_context.hpp"
#include "mamba/core/context.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
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
            auto spec = specs::ChannelSpec::parse(loc);
            auto channels = specs::Channel::resolve(std::move(spec), params);
            assert(channels.size() == 1);
            return std::move(channels.front());
        };

        auto make_simple_params_base(const Context& ctx) -> specs::ChannelResolveParams
        {
            return specs::ChannelResolveParams{
                /* .platform= */ create_platforms(ctx.platforms()),
                /* .channel_alias= */ specs::CondaURL::parse(util::path_or_url_to_url(ctx.channel_alias)),
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
    }

    ChannelContext::ChannelContext(Context& context)
        : m_channel_params(make_simple_params_base(context))
        , m_context(context)
    {
        add_conda_params_custom_channel(m_channel_params, m_context);
        add_simple_params_custom_multichannel(m_channel_params, m_context);
    }

    ChannelContext::ChannelContext(Context& context, ChannelResolveParams params)
        : m_channel_params(std::move(params))
        , m_context(context)
    {
    }

    auto ChannelContext::make_simple(Context& ctx) -> ChannelContext
    {
        auto params = make_simple_params_base(ctx);
        add_simple_params_custom_channel(params, ctx);
        add_simple_params_custom_multichannel(params, ctx);
        return { ctx, std::move(params) };
    }

    auto ChannelContext::make_conda_compatible(Context& ctx) -> ChannelContext
    {
        auto params = make_simple_params_base(ctx);
        add_conda_params_custom_channel(params, ctx);
        add_simple_params_custom_multichannel(params, ctx);
        return { ctx, std::move(params) };
    }

    auto ChannelContext::make_channel(std::string_view name) -> const channel_list&
    {
        if (const auto it = m_channel_cache.find(std::string(name)); it != m_channel_cache.end())
        {
            return it->second;
        }

        auto [it, inserted] = m_channel_cache.emplace(
            name,
            specs::Channel::resolve(specs::ChannelSpec::parse(name), params())
        );
        assert(inserted);
        return it->second;
    }

    auto ChannelContext::params() const -> const specs::ChannelResolveParams&
    {
        return m_channel_params;
    }

    auto ChannelContext::context() const -> const Context&
    {
        return m_context;
    }
}
