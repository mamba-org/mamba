// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>

#include "mamba/util/build.hpp"
#include "mamba/util/environment.hpp"
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
        if (
            starts_with(input, '~') || starts_with(input, '/') || (input == ".")
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

    auto path_win_detect_sep(std::string_view path) -> std::optional<char>
    {
        if (path_has_drive_letter(path))
        {
            return path.at(2);
        }
        for (const char sep : { preferred_path_separator_posix, preferred_path_separator_win })
        {
            const auto prefix = std::array<char, 2>{ '~', sep };
            const auto prefix_str = std::string_view(prefix.data(), prefix.size());
            if (starts_with(path, prefix_str))
            {
                return sep;
            }
        }
        if (path.find(preferred_path_separator_posix) < std::string_view::npos)
        {
            return preferred_path_separator_posix;
        }
        if (path.find(preferred_path_separator_win) < std::string_view::npos)
        {
            return preferred_path_separator_win;
        }
        return std::nullopt;
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

    auto path_posix_to_win(std::string path) -> std::string
    {
        std::replace(
            path.begin(),
            path.end(),
            preferred_path_separator_posix,
            preferred_path_separator_win
        );
        return path;
    }

    auto path_to_sep(std::string path, char sep) -> std::string
    {
        std::replace(path.begin(), path.end(), preferred_path_separator_posix, sep);
        std::replace(path.begin(), path.end(), preferred_path_separator_win, sep);
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
        auto parent_item = [&]()
        {
            if (parent_end < npos)
            {
                return parent.substr(parent_start, parent_end - parent_start);
            }
            return parent.substr(parent_start);
        };
        auto child_item = [&]()
        {
            if (child_end < npos)
            {
                return child.substr(child_start, child_end - child_start);
            }
            return child.substr(child_start);
        };
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
        return (parent_end == npos) && (parent_item().empty() || (parent_item() == child_item()));
    }

    auto path_concat(std::string_view parent, std::string_view child, char sep) -> std::string
    {
        if (parent.empty())
        {
            return std::string(child);
        }
        if (child.empty())
        {
            return std::string(parent);
        }
        return concat(rstrip(parent, sep), std::string_view(&sep, 1), strip(child, sep));
    }

    namespace
    {
        auto path_win_detect_sep_many(std::string_view first, std::string_view second) -> char
        {
            if (auto sep = path_win_detect_sep(first))
            {
                return sep.value();
            }
            if (auto sep = path_win_detect_sep(second))
            {
                return sep.value();
            }
            return preferred_path_separator_win;
        }
    }

    auto path_concat(std::string_view parent, std::string_view child) -> std::string
    {
        if (!on_win)
        {
            return path_concat(parent, child, preferred_path_separator_posix);
        }
        return path_concat(parent, child, path_win_detect_sep_many(parent, child));
    }

    auto expand_home(std::string_view path, std::string_view home, char sep) -> std::string
    {
        const auto prefix = std::array<char, 2>{ '~', sep };
        const auto prefix_str = std::string_view(prefix.data(), prefix.size());
        if ((path == "~") || starts_with(path, prefix_str))
        {
            return path_concat(home, path.substr(1), sep);
        }
        return std::string(path);
    }

    auto expand_home(std::string_view path, std::string_view home) -> std::string
    {
        if (!on_win)
        {
            return expand_home(path, home, preferred_path_separator_posix);
        }
        const auto sep = path_win_detect_sep_many(path, home);
        return expand_home(path, path_to_sep(std::string(home), sep), sep);
    }

    auto expand_home(std::string_view path) -> std::string
    {
        return expand_home(path, user_home_dir());
    }

    auto shrink_home(std::string_view path, std::string_view home, char sep) -> std::string
    {
        home = rstrip(home, sep);
        if (!home.empty() && path_is_prefix(home, path, sep))
        {
            return path_concat("~", path.substr(home.size()), sep);
        }
        return std::string(path);
    }

    auto shrink_home(std::string_view path, std::string_view home) -> std::string
    {
        if (!on_win)
        {
            return shrink_home(path, home, preferred_path_separator_posix);
        }
        const auto sep = path_win_detect_sep_many(path, home);
        return shrink_home(path, path_to_sep(std::string(home), sep), sep);
    }

    auto shrink_home(std::string_view path) -> std::string
    {
        return shrink_home(path, user_home_dir());
    }
}
