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
#include "mamba/util/url_manip.hpp"

namespace mamba
{

    /*********************************
     * ChannelContext implementation *
     *********************************/

    namespace
    {
        auto create_paltforms(const std::vector<std::string>& platforms)
            -> specs::ChannelResolveParams::platform_list
        {
            return { platforms.cbegin(), platforms.cend() };
        }
    }

    ChannelContext::ChannelContext(Context& context)
        : m_context(context)
    {
        m_channel_params.channel_alias = specs::CondaURL::parse(
            util::path_or_url_to_url(m_context.channel_alias)
        );
        m_channel_params.home_dir = util::user_home_dir();
        m_channel_params.current_working_dir = fs::current_path();
        m_channel_params.authentication_db = m_context.authentication_info();
        m_channel_params.platforms = create_paltforms(m_context.platforms());
        init_custom_channels();
    }

    void ChannelContext::init_custom_channels()
    {
        for (const auto& [name, location] : m_context.custom_channels)
        {
            auto channels = make_channel(location);
            assert(channels.size() == 1);
            channels.front().set_display_name(name);
            m_channel_params.custom_channels.emplace(name, std::move(channels.front()));
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
            m_channel_params.custom_multichannels.emplace(multi_name, std::move(channels));
        }
    }

    auto ChannelContext::make_channel(std::string_view name) -> channel_list
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
        return { it->second };
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
