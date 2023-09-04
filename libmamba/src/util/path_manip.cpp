// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>

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
}
