// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fmt/format.h>

#include "mamba/specs/archive.hpp"
#include "mamba/specs/channel_spec.hpp"
#include "mamba/specs/conda_url.hpp"
#include "mamba/util/string.hpp"

namespace mamba::specs
{
    namespace
    {
        auto parse_platform_list(std::string_view plats) -> util::flat_set<Platform>
        {
            static constexpr auto is_not_sep = [](char c) -> bool
            { return !util::contains(ChannelSpec::platform_separators, c); };

            auto out = util::flat_set<Platform>{};
            auto head_rest = util::lstrip_if_parts(plats, is_not_sep);
            while (!head_rest.front().empty())
            {
                if (auto p = platform_parse(head_rest.front()))
                {
                    out.insert(p.value());
                }
                else
                {
                    throw std::invalid_argument(
                        fmt::format(R"(Found invalid platform "{}")", head_rest.front())
                    );
                }
                head_rest = util::lstrip_if_parts(
                    util::lstrip(head_rest.back(), ChannelSpec::platform_separators),
                    is_not_sep
                );
            }
            return out;
        }

        auto parse_platform_path(std::string_view str)
            -> std::pair<std::string, std::optional<Platform>>
        {
            auto [pos, len, plat] = detail::find_slash_and_platform(str);
            if (plat.has_value())
            {
                return { util::concat(str.substr(0, pos), str.substr(pos + len)), plat.value() };
            }
            return { {}, std::nullopt };
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
                    std::string(str.substr(0, start_pos)),
                    parse_platform_list(str.substr(start_pos + 1, str.size() - start_pos - 2))
                );
            }
        }

        // For single archive channel specs, we don't need to compute platform filters
        // since they are not needed to compute URLs.
        if (has_archive_extension(str))
        {
            return { std::string(str), {} };
        }

        // Paring a platform inside a path name.
        // This is required because a channel can be instantiated from a value that already
        // contains the platform.
        auto [rest, plat] = parse_platform_path(str);
        if (plat.has_value())
        {
            return { std::move(rest), { plat.value() } };
        }

        return { std::string(str), {} };
    }

    ChannelSpec::ChannelSpec(std::string location, util::flat_set<Platform> filters)
        : m_location(std::move(location))
        , m_platform_filters(std::move(filters))
    {
        if (m_location.empty())
        {
            m_location = std::string(default_name);
        }
    }

    auto ChannelSpec::platform_filters() const -> const util::flat_set<Platform>&
    {
        return m_platform_filters;
    }
}
