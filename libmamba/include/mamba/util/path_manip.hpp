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
    /**
     * Lightweight file path manipulation.
     *
     * The purpose of this file is to provide a lightweight functions for manipulating paths
     * for things that manipulate "path-like" objects, such as parsers and URLs.
     * In general, users should prefer using the correct abstraction, such as @ref URL
     * and @ref u8path.
     * However some features provided here, such as @ref expand_home, are not available elsewhere.
     */

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
     * Detect the separator used in a path.
     */
    [[nodiscard]] auto path_win_detect_sep(std::string_view path) -> std::optional<char>;

    /**
     * Convert the Windows path separators to Posix ones.
     */
    [[nodiscard]] auto path_win_to_posix(std::string path) -> std::string;

    /**
     * Convert the Posix path separators to Windows ones.
     */
    [[nodiscard]] auto path_posix_to_win(std::string path) -> std::string;

    /**
     * Convert the path separators to the desired one.
     */
    [[nodiscard]] auto path_to_sep(std::string path, char sep) -> std::string;

    /**
     * Convert the Windows path separators to Posix ones on Windows only.
     */
    [[nodiscard]] auto path_to_posix(std::string path) -> std::string;

    /**
     * Check that a path is a prefix of another path.
     */
    [[nodiscard]] auto
    path_is_prefix(std::string_view parent, std::string_view child, char sep = '/') -> bool;

    /**
     * Concatenate paths with the given separator.
     */
    [[nodiscard]] auto path_concat(std::string_view parent, std::string_view child, char sep)
        -> std::string;

    /**
     * Concatenate paths with '/' on Unix and detected separator on Windows.
     */
    [[nodiscard]] auto path_concat(std::string_view parent, std::string_view child) -> std::string;

    /**
     * Expand a leading '~' with the given home directory, assuming the given separator.
     */
    [[nodiscard]] auto expand_home(std::string_view path, std::string_view home, char sep)
        -> std::string;

    /**
     * Expand a leading '~' with the given home directory.
     */
    [[nodiscard]] auto expand_home(std::string_view path, std::string_view home) -> std::string;

    /**
     * Expand a leading '~' with the user home directory.
     */
    [[nodiscard]] auto expand_home(std::string_view path) -> std::string;

    /**
     * If the path starts with the given home directory, replace it with a leading '~'.
     *
     * This assumes the given separator is used separate paths.
     */
    [[nodiscard]] auto shrink_home(std::string_view path, std::string_view home, char sep)
        -> std::string;

    /**
     * If the path starts with the given home directory, replace it with a leading '~'.
     */
    [[nodiscard]] auto shrink_home(std::string_view path, std::string_view home) -> std::string;

    /**
     * If the path starts with the user home directory, replace it with a leading '~'.
     */
    [[nodiscard]] auto shrink_home(std::string_view path) -> std::string;
}
#endif
