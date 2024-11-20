// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_ENVIRONMENT_HPP
#define MAMBA_UTIL_ENVIRONMENT_HPP

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"

namespace mamba::util
{
    /**
     * Get an environment variable encoded in UTF8.
     */
    [[nodiscard]] auto get_env(const std::string& key) -> std::optional<std::string>;

    /**
     * Set an environment variable encoded in UTF8.
     */
    void set_env(const std::string& key, const std::string& value);

    /**
     * Unset an environment variable encoded in UTF8.
     */
    void unset_env(const std::string& key);

    using environment_map = std::unordered_map<std::string, std::string>;

    /**
     * Return a map of all environment variables encoded in UTF8.
     *
     * This is useful if one is interested to do an operation over all environment variables
     * when their names are unknown.
     */
    [[nodiscard]] auto get_env_map() -> environment_map;

    /**
     * Equivalent to calling set_env in a loop.
     *
     * This leaves environment variables not referred to in the map unmodified.
     */
    void update_env_map(const environment_map& env);

    /**
     * Set the environment to be exactly the map given.
     *
     * This unsets all environment variables not referred to in the map unmodified.
     */
    void set_env_map(const environment_map& env);

    /*
     * Return the current user home directory.
     */
    [[nodiscard]] auto user_home_dir() -> std::string;

    /**
     * Return the current user config directory.
     *
     * On all platforms, the XDG_CONFIG_HOME environment variables are honored.
     * Otherwise, it returns the OS-specified config directory on Windows, and the XDG default
     * on Unix.
     */
    [[nodiscard]] auto user_config_dir() -> std::string;

    /**
     * Return the current user program data directory.
     *
     * On all platforms, the XDG_DATA_HOME environment variables are honored.
     * Otherwise, it returns the OS-specified config directory on Windows, and the XDG default
     * on Unix.
     */
    [[nodiscard]] auto user_data_dir() -> std::string;

    /**
     * Return the current user program dispensable cache directory.
     *
     * On all platforms, the XDG_CACHE_HOME environment variables are honored.
     * Otherwise, it returns the OS-specified config directory on Windows, and the XDG default
     * on Unix.
     */
    [[nodiscard]] auto user_cache_dir() -> std::string;

    /**
     * Return the character use to separate paths.
     */
    [[nodiscard]] constexpr auto pathsep() -> char;

    /**
     * Return directories of the given prefix path.
     */
    [[nodiscard]] auto get_path_dirs(const fs::u8path& prefix) -> std::vector<fs::u8path>;

    /**
     * Return the full path of a program from its name.
     */
    [[nodiscard]] auto which(std::string_view exe) -> fs::u8path;

    /**
     * Return the full path of a program from its name if found inside the given directories.
     */
    template <typename Iter>
    [[nodiscard]] auto which_in(std::string_view exe, Iter search_path_first, Iter search_path_last)
        -> fs::u8path;

    /**
     * Return the full path of a program from its name if found inside the given directories.
     *
     * The directories can be given as a range or as a @ref pathsep separated list.
     */
    template <typename Range>
    [[nodiscard]] auto which_in(std::string_view exe, const Range& search_paths) -> fs::u8path;

    /********************
     *  Implementation  *
     ********************/

    constexpr auto pathsep() -> char
    {
        if (on_win)
        {
            return ';';
        }
        else
        {
            return ':';
        }
    }

    namespace detail
    {
        [[nodiscard]] auto which_in_one(const fs::u8path& exe, const fs::u8path& dir) -> fs::u8path;

        [[nodiscard]] auto which_in_split(const fs::u8path& exe, std::string_view paths)
            -> fs::u8path;
    }

    template <typename Iter>
    auto which_in(std::string_view exe, Iter first, Iter last) -> fs::u8path
    {
        for (; first != last; ++first)
        {
            if (auto p = detail::which_in_one(exe, *first); !p.empty())
            {
                return p;
            }
        }
        return "";
    }

    template <typename Range>
    auto which_in(std::string_view exe, const Range& search_paths) -> fs::u8path
    {
        if constexpr (std::is_same_v<Range, fs::u8path>)
        {
            return detail::which_in_one(exe, search_paths);
        }
        else if constexpr (std::is_convertible_v<Range, std::string_view>)
        {
            return detail::which_in_split(exe, search_paths);
        }
        else
        {
            return which_in(exe, search_paths.cbegin(), search_paths.cend());
        }
    }
}
#endif
