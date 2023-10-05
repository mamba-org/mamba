// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_CHANNEL_HPP
#define MAMBA_CORE_CHANNEL_HPP

#include <map>
#include <memory>
#include <optional>
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

        std::string_view scheme() const;
        const std::string& location() const;
        const std::string& name() const;
        const std::string& canonical_name() const;
        const util::flat_set<std::string>& platforms() const;
        std::optional<std::string> auth() const;
        std::optional<std::string> user() const;
        std::optional<std::string> password() const;
        std::optional<std::string> token() const;
        std::optional<std::string> package_filename() const;
        const validation::RepoChecker&
        repo_checker(Context& context, MultiPackageCache& caches) const;

        std::string base_url() const;
        std::string platform_url(std::string platform, bool with_credential = true) const;
        // The pairs consist of (platform,url)
        util::flat_set<std::pair<std::string, std::string>>
        platform_urls(bool with_credential = true) const;
        util::flat_set<std::string> urls(bool with_credential = true) const;

    private:

        Channel(
            std::string_view scheme,
            std::string location,
            std::string name,
            std::string canonical_name,
            std::string_view user = {},
            std::string_view password = {},
            std::string_view token = {},
            std::string_view package_filename = {},
            util::flat_set<std::string> platforms = {}
        );

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

        // This is used to make sure that there is a unique repo for every channel
        mutable std::unique_ptr<validation::RepoChecker> p_repo_checker;

        // Note: as long as Channel is not a regular value-type and we want each
        // instance only possible to create through ChannelContext, we need
        // to have Channel's constructor only available to ChannelContext,
        // therefore enabling it's use through this `friend` statement.
        // However, all this should be removed as soon as Channel is changed to
        // be a regular value-type (regular as in the regular concept).
        friend class ChannelContext;
    };

    bool operator==(const Channel& lhs, const Channel& rhs);
    bool operator!=(const Channel& lhs, const Channel& rhs);


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

        const multichannel_map& get_custom_multichannels() const;

        Channel make_simple_channel(
            const specs::CondaURL& channel_alias,
            const std::string& channel_url,
            const std::string& channel_name,
            const std::string& channel_canonical_name
        );

        Channel from_url(specs::ChannelSpec&& url);
        Channel from_name(const std::string& name);
        Channel from_value(const std::string& value);
    };

}  // namespace mamba

#endif
