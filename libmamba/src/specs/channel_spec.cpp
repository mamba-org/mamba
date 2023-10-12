// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <utility>

#include <fmt/format.h>

#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/archive.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::specs
{
    namespace
    {
        using dynamic_platform_set = ChannelSpec::dynamic_platform_set;

        auto parse_platform_list(std::string_view plats) -> dynamic_platform_set
        {
            static constexpr auto is_not_sep = [](char c) -> bool
            { return !util::contains(ChannelSpec::platform_separators, c); };

            auto out = dynamic_platform_set{};
            auto head_rest = util::lstrip_if_parts(plats, is_not_sep);
            while (!head_rest.front().empty())
            {
                // Accepting all strings, so that user can dynamically register new platforms
                out.insert(util::to_lower(util::strip(head_rest.front())));
                head_rest = util::lstrip_if_parts(
                    util::lstrip(head_rest.back(), ChannelSpec::platform_separators),
                    is_not_sep
                );
            }
            return out;
        }

        auto parse_platform_path(std::string_view str) -> std::pair<std::string, std::string>
        {
            static constexpr auto npos = std::string_view::npos;

            auto [start, len, plat] = detail::find_slash_and_platform(str);
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
            -> std::pair<std::string, dynamic_platform_set>
        {
            if (util::ends_with(str, ']'))
            {
                // Parsing platforms in "something[linux-64,noarch]"
                const auto start_pos = str.find_last_of('[');
                if ((start_pos != std::string_view::npos) && (start_pos != 0))
                {
                    return {
                        std::string(util::rstrip(str.substr(0, start_pos))),
                        parse_platform_list(str.substr(start_pos + 1, str.size() - start_pos - 2)),
                    };
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
                    return {
                        std::move(rest),
                        { std::move(plat) },
                    };
                }
            }

            // For single archive channel specs, we don't need to compute platform filters
            // since they are not needed to compute URLs.
            return { std::string(util::rstrip(str)), {} };
        }

        auto parse_path(std::string_view str) -> std::string
        {
            auto out = util::path_to_posix(fs::u8path(str).lexically_normal().string());
            out = util::rstrip(out, '/');
            return out;
        }
    }

    auto ChannelSpec::parse(std::string_view str) -> ChannelSpec
    {
        str = util::strip(str);
        if (str.empty())
        {
            return {};
        }

        auto [location, filters] = split_location_platform(str);

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

        return { std::move(location), std::move(filters), type };
    }

    ChannelSpec::ChannelSpec(std::string location, dynamic_platform_set filters, Type type)
        : m_location(std::move(location))
        , m_platform_filters(std::move(filters))
        , m_type(type)
    {
        if (m_location.empty())
        {
            m_location = std::string(default_name);
        }
    }

    auto ChannelSpec::type() const -> Type
    {
        return m_type;
    }

    auto ChannelSpec::location() const& -> const std::string&
    {
        return m_location;
    }

    auto ChannelSpec::location() && -> std::string
    {
        return std::move(m_location);
    }

    auto ChannelSpec::clear_location() -> std::string
    {
        return std::exchange(m_location, "");
    }

    auto ChannelSpec::platform_filters() const& -> const dynamic_platform_set&
    {
        return m_platform_filters;
    }

    auto ChannelSpec::platform_filters() && -> dynamic_platform_set
    {
        return std::move(m_platform_filters);
    }

    auto ChannelSpec::clear_platform_filters() -> dynamic_platform_set
    {
        return std::exchange(m_platform_filters, {});
    }
}
