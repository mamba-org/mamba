// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_CHANNEL_SPEC_HPP
#define MAMBA_SPECS_CHANNEL_SPEC_HPP

#include <string>
#include <string_view>

#include "mamba/specs/platform.hpp"
#include "mamba/util/flat_set.hpp"

namespace mamba::specs
{
    /**
     * Channel specification.
     *
     * This represent the string that is passed by the user to select a channel.
     * It needs to be resolved in order to get a final URL/path.
     * This is even true when a full URL or path is given, as some authentification information
     * may come from channel alias or login database.
     */
    class ChannelSpec
    {
    public:

        inline static constexpr std::string_view default_name = "defaults";
        inline static constexpr std::string_view platform_separators = "|,;";

        [[nodiscard]] static auto parse(std::string_view str) -> ChannelSpec;

        ChannelSpec() = default;
        ChannelSpec(std::string location, util::flat_set<Platform> filters);

        [[nodiscard]] auto platform_filters() const -> const util::flat_set<Platform>&;


    private:

        std::string m_location = std::string(default_name);
        util::flat_set<Platform> m_platform_filters = {};
    };
}
#endif
