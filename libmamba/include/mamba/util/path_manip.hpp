// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_PATH_MANIP_HPP
#define MAMBA_UTIL_PATH_MANIP_HPP

#include <optional>
#include <string>
#include <string_view>

namespace mamba::util
{
    inline static constexpr char preferred_path_separator_posix = '/';
    inline static constexpr char preferred_path_separator_win = '\\';

    /**
     * Return true is the input is explicitly a path.
     *
     * Explicit path are:
     * - Absolute path
     * - Path starting with '~'
     * - Relative paths starting with "./" or "../"
     */
    [[nodiscard]] auto is_explicit_path(std::string_view input) -> bool;

    /**
     * Return the path drive letter, if any, or none.
     */
    [[nodiscard]] auto path_get_drive_letter(std::string_view path) -> std::optional<char>;

    /**
     * Check if a Windows path (not URL) starts with a drive letter.
     */
    [[nodiscard]] auto path_has_drive_letter(std::string_view path) -> bool;

    /**
     * Convert the Windows path separators to Posix ones.
     */
    [[nodiscard]] auto path_win_to_posix(std::string path) -> std::string;

    /**
     * Convert the Windows path separators to Posix ones on Windows only.
     */
    [[nodiscard]] auto path_to_posix(std::string path) -> std::string;

    /**
     * Check that a path is a prefix of another path.
     */
    [[nodiscard]] auto
    path_is_prefix(std::string_view parent, std::string_view child, char sep = '/') -> bool;
}
#endif
