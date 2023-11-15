// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CHANNEL_HPP
#define MAMBA_CORE_CHANNEL_HPP

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "mamba/specs/conda_url.hpp"
#include "mamba/util/flat_set.hpp"

namespace mamba
{
    class Context;
    class MultiPackageCache;
    namespace validation
    {
        class RepoChecker;
    }
    namespace specs
    {
        class ChannelSpec;
        class AuthenticationDataBase;
    }

    // Note: Channels can only be created using ChannelContext.
    class Channel
    {
    public:

        struct ResolveParams
        {
            using platform_list = util::flat_set<std::string>;
            using channel_list = std::vector<Channel>;
            using channel_map = std::map<std::string, Channel>;
            using multichannel_map = std::unordered_map<std::string, channel_list>;

            const platform_list& platforms;
            const specs::CondaURL& channel_alias;
            const channel_map& custom_channels;
            const specs::AuthenticationDataBase& auth_db;

            // TODO add CWD and home
        };

        using platform_list = util::flat_set<std::string>;

        Channel(specs::CondaURL url, std::string display_name, util::flat_set<std::string> platforms = {});

        [[nodiscard]] auto url() const -> const specs::CondaURL&;
        void set_url(specs::CondaURL url);

        [[nodiscard]] auto platforms() const -> const platform_list&;
        void set_platforms(platform_list platforms);

        [[nodiscard]] auto display_name() const -> const std::string&;
        void set_display_name(std::string display_name);

        std::string base_url() const;
        std::string platform_url(std::string_view platform, bool with_credential = true) const;
        // The pairs consist of (platform,url)
        util::flat_set<std::pair<std::string, std::string>>
        platform_urls(bool with_credential = true) const;
        util::flat_set<std::string> urls(bool with_credential = true) const;

    private:

        specs::CondaURL m_url;
        std::string m_display_name;
        util::flat_set<std::string> m_platforms;
    };

    /** Tuple-like equality of all observable members */
    auto operator==(const Channel& a, const Channel& b) -> bool;
    auto operator!=(const Channel& a, const Channel& b) -> bool;
}

template <>
struct std::hash<mamba::Channel>
{
    auto operator()(const mamba::Channel& c) const -> std::size_t;
};


namespace mamba
{

    class ChannelContext
    {
    public:

        using channel_map = std::map<std::string, Channel>;
        using channel_list = std::vector<Channel>;
        using multichannel_map = std::unordered_map<std::string, channel_list>;

        ChannelContext(Context& context);
        ~ChannelContext();

        ChannelContext(const ChannelContext&) = delete;
        ChannelContext& operator=(const ChannelContext&) = delete;
        ChannelContext(ChannelContext&&) = delete;
        ChannelContext& operator=(ChannelContext&&) = delete;

        const Channel& make_channel(const std::string& value);
        auto get_channels(const std::vector<std::string>& channel_names) -> channel_list;

        const specs::CondaURL& get_channel_alias() const;
        const channel_map& get_custom_channels() const;
        const multichannel_map& get_custom_multichannels() const;

        Context& context() const
        {
            return m_context;
        }

    private:

        using ChannelCache = std::map<std::string, Channel>;

        Context& m_context;
        ChannelCache m_channel_cache;
        specs::CondaURL m_channel_alias;
        channel_map m_custom_channels;
        multichannel_map m_custom_multichannels;

        void init_custom_channels();

        Channel from_value(const std::string& value);
    };

}  // namespace mamba

#endif
