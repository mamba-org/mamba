// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SPECS_UNRESOLVED_CHANNEL_HPP
#define MAMBA_SPECS_UNRESOLVED_CHANNEL_HPP

#include <array>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <fmt/format.h>

#include "mamba/specs/error.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/util/flat_set.hpp"
#include "mamba/util/tuple_hash.hpp"

namespace mamba::specs
{
    /**
     * Unresolved Channel specification.
     *
     * This represent an unverified channel string passed by the user, or written through files.
     * Due the the heavy reliance of channels on configuration options, this placeholder type
     * can be used to represent channel inputs that have not been "resolved" to s specific
     * location.
     * This can even be true when a full URL or path is given, as some authentication information
     * may come from login database.
     *
     * Note that for a string to be considered a URL, it must have an explicit scheme.
     * So "repo.anaconda.com" is considered a name, similarly to "conda-forge" and not a URL.
     * This is because otherwise it is not possible to tell names and URL apart.
     */
    class UnresolvedChannel
    {
    public:

        enum class Type
        {
            /**
             * A URL to a full repo structure.
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
             * An (possibly implicit) path to a full repo structure.
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

        using platform_set = util::flat_set<DynamicPlatform>;

        [[nodiscard]] static auto parse_platform_list(std::string_view plats) -> platform_set;

        [[nodiscard]] static auto parse(std::string_view str) -> expected_parse_t<UnresolvedChannel>;

        UnresolvedChannel() = default;
        UnresolvedChannel(std::string location, platform_set filters, Type type);

        [[nodiscard]] auto type() const -> Type;

        [[nodiscard]] auto location() const& -> const std::string&;
        [[nodiscard]] auto location() && -> std::string;
        auto clear_location() -> std::string;

        [[nodiscard]] auto platform_filters() const& -> const platform_set&;
        [[nodiscard]] auto platform_filters() && -> platform_set;
        auto clear_platform_filters() -> platform_set;

        [[nodiscard]] auto is_package() const -> bool;

        [[nodiscard]] auto str() const -> std::string;

        [[nodiscard]] auto operator==(const UnresolvedChannel& other) const -> bool
        {
            return m_location == other.m_location                     //
                   && m_platform_filters == other.m_platform_filters  //
                   && m_type == other.m_type;
        }

        [[nodiscard]] auto operator!=(const UnresolvedChannel& other) const -> bool
        {
            return !(*this == other);
        }

    private:

        std::string m_location = std::string(unknown_channel);
        platform_set m_platform_filters = {};
        Type m_type = Type::Unknown;
    };
}

template <>
struct fmt::formatter<mamba::specs::UnresolvedChannel>
{
    using UnresolvedChannel = ::mamba::specs::UnresolvedChannel;

    constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
    {
        // make sure that range is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}')
        {
            throw fmt::format_error("Invalid format");
        }
        return ctx.begin();
    }

    auto format(const UnresolvedChannel& uc, format_context& ctx) const -> format_context::iterator;
};

template <>
struct std::hash<mamba::specs::UnresolvedChannel>
{
    auto operator()(const mamba::specs::UnresolvedChannel& uc) const -> std::size_t
    {
        return mamba::util::hash_vals(uc.location(), uc.platform_filters(), static_cast<int>(uc.type()));
    }
};

#endif
