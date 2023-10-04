// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <utility>

#include <fmt/format.h>

#include "mamba/core/environment.hpp"
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
        auto parse_platform_list(std::string_view plats) -> util::flat_set<std::string>
        {
            static constexpr auto is_not_sep = [](char c) -> bool
            { return !util::contains(ChannelSpec::platform_separators, c); };

            auto out = util::flat_set<std::string>{};
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

        auto parse_location(std::string_view str) -> std::string
        {
            if (util::is_explicit_path(str))
            {
                auto out = util::path_to_url(str);
                out = util::rstrip(out, '/');
                return out;
            }
            return std::string(util::rstrip(str, '/'));
        }
    }

    auto ChannelSpec::parse(std::string_view str) -> ChannelSpec
    {
        str = util::strip(str);
        if (str.empty())
        {
            return {};
        }

        // Parsing platforms in "something[linux-64,noarch]"
        if (util::ends_with(str, ']'))
        {
            const auto start_pos = str.find_last_of('[');
            if ((start_pos != std::string_view::npos) && (start_pos != 0))
            {
                return ChannelSpec(
                    parse_location(str.substr(0, start_pos)),
                    parse_platform_list(str.substr(start_pos + 1, str.size() - start_pos - 2))
                );
            }
        }

        // For single archive channel specs, we don't need to compute platform filters
        // since they are not needed to compute URLs.
        if (has_archive_extension(str))
        {
            return { parse_location(str), {} };
        }

        // Paring a platform inside a path name.
        // This is required because a channel can be instantiated from a value that already
        // contains the platform.
        auto [rest, plat] = parse_platform_path(str);
        if (!plat.empty())
        {
            return { parse_location(rest), { std::move(plat) } };
        }

        return { parse_location(str), {} };
    }

    ChannelSpec::ChannelSpec(std::string location, util::flat_set<std::string> filters)
        : m_location(std::move(location))
        , m_platform_filters(std::move(filters))
    {
        if (m_location.empty())
        {
            m_location = std::string(default_name);
        }
    }

    auto ChannelSpec::type() const -> Type
    {
        const bool has_package = has_archive_extension(m_location);
        const std::string_view scheme = util::url_get_scheme(m_location);

        if (scheme == "file")
        {
            return has_package ? Type::PackagePath : Type::Path;
        }
        if (!scheme.empty())
        {
            return has_package ? Type::PackageURL : Type::URL;
        }
        return Type::Name;
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

    auto ChannelSpec::platform_filters() const& -> const util::flat_set<std::string>&
    {
        return m_platform_filters;
    }

    auto ChannelSpec::platform_filters() && -> util::flat_set<std::string>
    {
        return std::move(m_platform_filters);
    }

    auto ChannelSpec::clear_platform_filters() -> util::flat_set<std::string>
    {
        return std::exchange(m_platform_filters, {});
    }
}
