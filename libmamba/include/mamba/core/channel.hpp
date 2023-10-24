// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CHANNEL_HPP
#define MAMBA_CORE_CHANNEL_HPP

#include <map>
#include <string>
#include <string_view>
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
    }

    std::vector<std::string> get_known_platforms();

    // Note: Channels can only be created using ChannelContext.
    class Channel
    {
    public:

        Channel(const Channel&) = delete;
        Channel& operator=(const Channel&) = delete;
        Channel(Channel&&) noexcept = default;
        Channel& operator=(Channel&&) noexcept = default;

        ~Channel();

        const std::string& location() const;
        const std::string& name() const;
        const std::string& canonical_name() const;
        const util::flat_set<std::string>& platforms() const;
        const specs::CondaURL& url() const;

        std::string base_url() const;
        std::string platform_url(std::string_view platform, bool with_credential = true) const;
        // The pairs consist of (platform,url)
        util::flat_set<std::pair<std::string, std::string>>
        platform_urls(bool with_credential = true) const;
        util::flat_set<std::string> urls(bool with_credential = true) const;

    private:

        Channel(
            specs::CondaURL url,
            std::string location,
            std::string name,
            std::string canonical_name,
            util::flat_set<std::string> platforms = {}
        );

        specs::CondaURL m_url;
        std::string m_location;
        std::string m_name;
        std::string m_canonical_name;
        util::flat_set<std::string> m_platforms;

        // Note: as long as Channel is not a regular value-type and we want each
        // instance only possible to create through ChannelContext, we need
        // to have Channel's constructor only available to ChannelContext,
        // therefore enabling it's use through this `friend` statement.
        // However, all this should be removed as soon as Channel is changed to
        // be a regular value-type (regular as in the regular concept).
        friend class ChannelContext;
    };

    using ChannelCache = std::map<std::string, Channel>;

    class ChannelContext
    {
    public:

        using channel_list = std::vector<std::string>;
        using channel_map = std::map<std::string, Channel>;
        using multichannel_map = std::map<std::string, std::vector<std::string>>;

        ChannelContext(Context& context);
        ~ChannelContext();

        ChannelContext(const ChannelContext&) = delete;
        ChannelContext& operator=(const ChannelContext&) = delete;
        ChannelContext(ChannelContext&&) = delete;
        ChannelContext& operator=(ChannelContext&&) = delete;

        const Channel& make_channel(const std::string& value);
        std::vector<const Channel*> get_channels(const std::vector<std::string>& channel_names);

        const specs::CondaURL& get_channel_alias() const;
        const channel_map& get_custom_channels() const;
        const multichannel_map& get_custom_multichannels() const;

        Context& context() const
        {
            return m_context;
        }

    private:

        Context& m_context;
        ChannelCache m_channel_cache;
        specs::CondaURL m_channel_alias;
        channel_map m_custom_channels;
        multichannel_map m_custom_multichannels;

        void init_custom_channels();

        Channel from_any_path(specs::ChannelSpec&& spec);
        Channel from_package_path(specs::ChannelSpec&& spec);
        Channel from_path(specs::ChannelSpec&& spec);
        Channel from_any_url(specs::ChannelSpec&& spec);
        Channel from_package_url(specs::ChannelSpec&& spec);
        Channel from_url(specs::ChannelSpec&& spec);
        Channel from_name(specs::ChannelSpec&& spec);
        Channel from_value(const std::string& value);
    };

}  // namespace mamba

#endif
