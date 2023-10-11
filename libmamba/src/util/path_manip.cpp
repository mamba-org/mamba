// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>

#include "mamba/util/build.hpp"
#include "mamba/util/path_manip.hpp"
#include "mamba/util/string.hpp"
#include "mamba/util/url_manip.hpp"

namespace mamba::util
{
    auto is_explicit_path(std::string_view input) -> bool
    {
        // URI are not path
        if (url_has_scheme(input))
        {
            return false;
        }
        // Posix-like path
        if (starts_with(input, '~') || starts_with(input, '/') || (input == ".")
            || starts_with(input, "./") || (input == "..") || starts_with(input, "../")

        )
        {
            return true;
        }
        // Windows like path
        if ((input.size() >= 3) && is_alpha(input[0]) && (input[1] == ':')
            && ((input[2] == '/') || (input[2] == '\\')))
        {
            return true;
        }
        return false;
    }

    auto path_get_drive_letter(std::string_view path) -> std::optional<char>
    {
        if (path_has_drive_letter(path))
        {
            return { path.front() };
        }
        return std::nullopt;
    }

    auto path_has_drive_letter(std::string_view path) -> bool
    {
        return (path.size() >= 3) && is_alpha(path[0]) && (path[1] == ':')
               && ((path[2] == '/') || (path[2] == '\\'));
    }

    auto path_win_to_posix(std::string path) -> std::string
    {
        std::replace(
            path.begin(),
            path.end(),
            preferred_path_separator_win,
            preferred_path_separator_posix
        );
        return path;
    }

    auto path_to_posix(std::string path) -> std::string
    {
        if (on_win)
        {
            return path_win_to_posix(std::move(path));
        }
        return path;
    }

    // TODO(C++20): Use std::ranges::split_view
    auto path_is_prefix(std::string_view parent, std::string_view child, char sep) -> bool
    {
        static constexpr auto npos = std::string_view::npos;

        std::size_t parent_start = 0;
        std::size_t parent_end = parent.find(sep);
        std::size_t child_start = 0;
        std::size_t child_end = child.find(sep);
        auto parent_item = [&]() { return parent.substr(parent_start, parent_end); };
        auto child_item = [&]() { return child.substr(child_start, child_end); };
        while ((parent_end != npos) && (child_end != npos))
        {
            if (parent_item() != child_item())
            {
                return false;
            }
            parent_start = parent_end + 1;
            parent_end = parent.find(sep, parent_start);
            child_start = child_end + 1;
            child_end = child.find(sep, child_start);
        }
        // Last item comparison
        return parent_item().empty() || (parent_item() == child_item());
    }
}
