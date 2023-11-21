// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mamba/specs/authentication_info.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/flat_set.hpp"
#include "mamba/util/weakening_map.hpp"

#ifndef MAMBA_SPECS_CHANNEL_HPP
#define MAMBA_SPECS_CHANNEL_HPP

namespace mamba::specs
{
    class Channel;

    struct ChannelResolveParams
    {
        /**
         * The weakener for @ref ResolveParams::custom_channels.
         */
        struct NameWeakener
        {
            /**
             * Return the key unchanged.
             */
            [[nodiscard]] auto make_first_key(std::string_view key) const -> std::string_view;

            /**
             * Remove the last element of the '/'-separated name.
             */
            [[nodiscard]] auto weaken_key(std::string_view key) const
                -> std::optional<std::string_view>;
        };

        template <typename Key, typename Value>
        using name_map = util::weakening_map<std::unordered_map<Key, Value>, NameWeakener>;

        using platform_list = util::flat_set<std::string>;
        using channel_list = std::vector<Channel>;
        using channel_map = name_map<std::string, Channel>;
        using multichannel_map = name_map<std::string, channel_list>;

        const platform_list& platforms;
        const specs::CondaURL& channel_alias;
        const channel_map& custom_channels;
        const multichannel_map& custom_multichannels;
        const specs::AuthenticationDataBase& authentication_db;
        std::string_view home_dir;
        std::string_view current_working_dir;
    };

    class Channel
    {
    public:

        using platform_list = ChannelResolveParams::platform_list;
        using channel_list = ChannelResolveParams::channel_list;

        [[nodiscard]] static auto resolve(specs::ChannelSpec spec, ChannelResolveParams params)
            -> channel_list;

        Channel(specs::CondaURL url, std::string display_name, util::flat_set<std::string> platforms = {});

        [[nodiscard]] auto url() const -> const specs::CondaURL&;
        auto clear_url() -> const specs::CondaURL;
        void set_url(specs::CondaURL url);

        [[nodiscard]] auto platforms() const -> const platform_list&;
        auto clear_platforms() -> platform_list;
        void set_platforms(platform_list platforms);

        [[nodiscard]] auto display_name() const -> const std::string&;
        auto clear_display_name() -> std::string;
        void set_display_name(std::string display_name);

        [[nodiscard]] auto url_equivalent_with(const Channel& other) const -> bool;

        [[nodiscard]] auto is_equivalent_to(const Channel& other) const -> bool;

        [[nodiscard]] auto contains_equivalent(const Channel& other) const -> bool;

        [[nodiscard]] auto
        platform_url(std::string_view platform, bool with_credential = true) const -> std::string;
        // The pairs consist of (platform,url)
        [[nodiscard]] auto platform_urls(bool with_credential = true) const
            -> util::flat_set<std::pair<std::string, std::string>>;
        [[nodiscard]] auto urls(bool with_credential = true) const -> util::flat_set<std::string>;

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
struct std::hash<mamba::specs::Channel>
{
    auto operator()(const mamba::specs::Channel& c) const -> std::size_t;
};

#endif
