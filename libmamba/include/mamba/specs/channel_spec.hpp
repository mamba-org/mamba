// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_CHANNEL_SPEC_HPP
#define MAMBA_SPECS_CHANNEL_SPEC_HPP

#include <array>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/format.h>

#include "mamba/util/flat_set.hpp"

namespace mamba::specs
{
    /**
     * Channel specification.
     *
     * This represent the string that is passed by the user to select a channel.
     * It needs to be resolved in order to get a final URL/path.
     * This is even true when a full URL or path is given, as some authentification information
     * may come from login database.
     *
     * Note that for a string to be considered a URL, it must have an explicit scheme.
     * So "repo.anaconda.com" is considered a name, similarily to "conda-forge" and not a URL.
     * This is because otherwise it is not possible to tell names and URL appart.
     */
    class ChannelSpec
    {
    public:

        enum class Type
        {
            /**
             * A URL to a full repo strucuture.
             *
             * Example "https://repo.anaconda.com/conda-forge".
             */
            URL,
            /**
             * A URL to a single package.
             *
             * Example "https://repo.anaconda.com/conda-forge/linux-64/pkg-0.0-bld.conda".
             */
            PackageURL,
            /**
             * An (possibly implicit) path to a full repo strucuture.
             *
             * Example "/Users/name/conda-bld", "./conda-bld", "~/.conda-bld".
             */
            Path,
            /**
             * An (possibly implicit) path to a single-package.
             *
             * Example "/tmp/pkg-0.0-bld.conda", "./pkg-0.0-bld.conda", "~/pkg-0.0-bld.tar.bz2".
             */
            PackagePath,
            /**
             * A relative name.
             *
             * It needs to be resolved using a channel alias, or a custom channel.
             * Example "conda-forge", "locals", "my-channel/my-label".
             */
            Name,
            /**
             * An unknown channel source.
             *
             * It is currently unclear why it is needed.
             */
            Unknown,
        };

        static constexpr std::string_view platform_separators = "|,;";
        static constexpr std::string_view unknown_channel = "<unknown>";
        static constexpr std::array<std::string_view, 4> invalid_channels_lower = {
            "<unknown>",
            "none:///<unknown>",
            "none",
            ":///<unknown>",
        };

        using dynamic_platform_set = util::flat_set<std::string>;

        [[nodiscard]] static auto parse(std::string_view str) -> ChannelSpec;

        ChannelSpec() = default;
        ChannelSpec(std::string location, dynamic_platform_set filters, Type type);

        [[nodiscard]] auto type() const -> Type;

        [[nodiscard]] auto location() const& -> const std::string&;
        [[nodiscard]] auto location() && -> std::string;
        auto clear_location() -> std::string;

        [[nodiscard]] auto platform_filters() const& -> const dynamic_platform_set&;
        [[nodiscard]] auto platform_filters() && -> dynamic_platform_set;
        auto clear_platform_filters() -> dynamic_platform_set;

        [[nodiscard]] auto str() const -> std::string;

    private:

        std::string m_location = std::string(unknown_channel);
        dynamic_platform_set m_platform_filters = {};
        Type m_type = Type::Unknown;
    };
}

template <>
struct fmt::formatter<mamba::specs::ChannelSpec>
{
    using ChannelSpec = ::mamba::specs::ChannelSpec;

    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        // make sure that range is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
        {
            throw fmt::format_error("Invalid format");
        }
        return ctx.begin();
    }

    auto format(const ChannelSpec& spec, format_context& ctx) const -> format_context::iterator;
};

#endif
