// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <fmt/format.h>

#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/specs/platform.hpp"
#include "mamba/specs/unresolved_channel.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::specs
{
    // Defined in  conda_url.cpp
    [[nodiscard]] auto find_slash_and_platform(std::string_view path)
        -> std::tuple<std::size_t, std::size_t, std::optional<KnownPlatform>>;

    auto UnresolvedChannel::parse_platform_list(std::string_view plats) -> platform_set
    {
        static constexpr auto is_not_sep = [](char c) -> bool
        { return !util::contains(UnresolvedChannel::platform_separators, c); };

        auto out = platform_set{};
        auto head_rest = util::lstrip_if_parts(plats, is_not_sep);
        while (!head_rest.front().empty())
        {
            // Accepting all strings, so that user can dynamically register new platforms
            out.insert(util::to_lower(util::strip(head_rest.front())));
            head_rest = util::lstrip_if_parts(
                util::lstrip(head_rest.back(), UnresolvedChannel::platform_separators),
                is_not_sep
            );
        }
        return out;
    }

    namespace
    {
        using dynamic_platform_set = UnresolvedChannel::platform_set;

        auto parse_platform_path(std::string_view str) -> std::pair<std::string, DynamicPlatform>
        {
            static constexpr auto npos = std::string_view::npos;

            auto [start, len, plat] = find_slash_and_platform(str);
            if (plat.has_value())
            {
                const auto end = (len == npos) ? str.size() : start + len;
                return {
                    util::concat(str.substr(0, start), str.substr(end)),
                    std::string(platform_name(plat.value())),
                };
            }
            return { {}, {} };
        }

        auto split_location_platform(std::string_view str)
            -> expected_parse_t<std::pair<std::string, dynamic_platform_set>>
        {
            if (util::ends_with(str, ']'))
            {
                // Parsing platforms in "something[linux-64,noarch]"
                const auto start_pos = str.find_last_of('[');
                if ((start_pos != std::string_view::npos) && (start_pos != 0))
                {
                    return { {
                        std::string(util::rstrip(str.substr(0, start_pos))),
                        UnresolvedChannel::parse_platform_list(
                            str.substr(start_pos + 1, str.size() - start_pos - 2)
                        ),
                    } };
                }
                else
                {
                    return make_unexpected_parse(R"(Unexpected closing backet "]")");
                }
            }

            if (!has_archive_extension(str))
            {
                // Paring a platform inside a path name.
                // This is required because a channel can be instantiated from a value that already
                // contains the platform.
                auto [rest, plat] = parse_platform_path(str);
                if (!plat.empty())
                {
                    rest = util::rstrip(rest, '/');
                    return { {
                        std::move(rest),
                        { std::move(plat) },
                    } };
                }
            }

            // For single archive channel specs, we don't need to compute platform filters
            // since they are not needed to compute URLs.
            return { { std::string(util::rstrip(str)), {} } };
        }

        auto parse_path(std::string_view str) -> std::string
        {
            auto out = util::path_to_posix(fs::u8path(str).lexically_normal().string());
            out = util::rstrip(out, '/');
            return out;
        }

        auto is_unknown_channel(std::string_view str) -> bool
        {
            auto it = std::find(
                UnresolvedChannel::invalid_channels_lower.cbegin(),
                UnresolvedChannel::invalid_channels_lower.cend(),
                util::to_lower(str)
            );
            return str.empty() || (it != UnresolvedChannel::invalid_channels_lower.cend());
        }
    }

    auto UnresolvedChannel::parse(std::string_view str) -> expected_parse_t<UnresolvedChannel>
    {
        str = util::strip(str);
        if (is_unknown_channel(str))
        {
            return { { std::string(unknown_channel), {}, Type::Unknown } };
        }

        auto location = std::string();
        auto filters = platform_set();
        auto split_outcome = split_location_platform(str);
        if (split_outcome)
        {
            std::tie(location, filters) = std::move(split_outcome).value();
        }
        else
        {
            return make_unexpected_parse(
                fmt::format(R"(Error parsing channel "{}": {})", str, split_outcome.error().what())
            );
        }

        const std::string_view scheme = util::url_get_scheme(location);
        Type type = {};
        if (scheme == "file")
        {
            type = has_archive_extension(location) ? Type::PackagePath : Type::Path;
        }
        else if (!scheme.empty())
        {
            type = has_archive_extension(location) ? Type::PackageURL : Type::URL;
        }
        else if (util::is_explicit_path(location))
        {
            location = parse_path(location);
            type = has_archive_extension(location) ? Type::PackagePath : Type::Path;
        }
        else
        {
            type = Type::Name;
        }

        return { { std::move(location), std::move(filters), type } };
    }

    UnresolvedChannel::UnresolvedChannel(std::string location, platform_set filters, Type type)
        : m_location(std::move(location))
        , m_platform_filters(std::move(filters))
        , m_type(type)
    {
        if (m_type == Type::Unknown)
        {
            m_location = unknown_channel;
            // Allowing in any platform filters for unkown type can be useful in MatchSpec
        }
        if (m_location.empty())
        {
            throw std::invalid_argument(  //
                "Cannot channel with empty location, "
                "use unknown type instead."
            );
        }
    }

    auto UnresolvedChannel::type() const -> Type
    {
        return m_type;
    }

    auto UnresolvedChannel::location() const& -> const std::string&
    {
        return m_location;
    }

    auto UnresolvedChannel::location() && -> std::string
    {
        return std::move(m_location);
    }

    auto UnresolvedChannel::clear_location() -> std::string
    {
        return std::exchange(m_location, "");
    }

    auto UnresolvedChannel::platform_filters() const& -> const platform_set&
    {
        return m_platform_filters;
    }

    auto UnresolvedChannel::platform_filters() && -> platform_set
    {
        return std::move(m_platform_filters);
    }

    auto UnresolvedChannel::clear_platform_filters() -> platform_set
    {
        return std::exchange(m_platform_filters, {});
    }

    auto UnresolvedChannel::str() const -> std::string
    {
        return fmt::format("{}", *this);
    }
}

auto
fmt::formatter<mamba::specs::UnresolvedChannel>::format(const UnresolvedChannel& uc, format_context& ctx) const
    -> format_context::iterator
{
    auto out = fmt::format_to(ctx.out(), "{}", uc.location());
    if (!uc.platform_filters().empty())
    {
        out = fmt::format_to(ctx.out(), "[{}]", fmt::join(uc.platform_filters(), ","));
    }
    return out;
}
