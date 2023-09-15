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
#include <vector>


namespace mamba
{
    class Context;
    class MultiPackageCache;
    namespace validation
    {
        class RepoChecker;
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

        const std::string& scheme() const;
        const std::string& location() const;
        const std::string& name() const;
        const std::string& canonical_name() const;
        const std::vector<std::string>& platforms() const;
        const std::optional<std::string>& auth() const;
        const std::optional<std::string>& token() const;
        const std::optional<std::string>& package_filename() const;
        const validation::RepoChecker&
        repo_checker(Context& context, MultiPackageCache& caches) const;

        std::string base_url() const;
        std::string platform_url(std::string platform, bool with_credential = true) const;
        // The pairs consist of (platform,url)
        std::vector<std::pair<std::string, std::string>>
        platform_urls(bool with_credential = true) const;
        std::vector<std::string> urls(bool with_credential = true) const;

    private:

        Channel(
            std::string scheme,
            std::string location,
            std::string name,
            std::string canonical_name,
            std::optional<std::string> auth = {},
            std::optional<std::string> token = {},
            std::optional<std::string> package_filename = {}
        );

        std::string m_scheme;
        std::string m_location;
        std::string m_name;
        std::string m_canonical_name;
        std::vector<std::string> m_platforms;
        std::optional<std::string> m_auth;
        std::optional<std::string> m_token;
        std::optional<std::string> m_package_filename;

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

        const Channel& get_channel_alias() const;
        const channel_map& get_custom_channels() const;

        Context& context() const
        {
            return m_context;
        }

    private:

        Context& m_context;
        ChannelCache m_channel_cache;
        Channel m_channel_alias;
        channel_map m_custom_channels;
        multichannel_map m_custom_multichannels;

        void init_custom_channels();

        const multichannel_map& get_custom_multichannels() const;

        Channel make_simple_channel(
            const Channel& channel_alias,
            const std::string& channel_url,
            const std::string& channel_name,
            const std::string& channel_canonical_name
        );

        Channel from_url(const std::string& url);
        Channel from_name(const std::string& name);
        Channel from_value(const std::string& value);
        Channel from_alias(std::string_view alias);
    };

}  // namespace mamba

#endif
