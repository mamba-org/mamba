// Copyright (c) 2019-2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_ENVIRONMENT_HPP
#define MAMBA_CORE_ENVIRONMENT_HPP

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"

namespace mamba::env
{
    [[nodiscard]] constexpr auto pathsep() -> const char*;

    auto get(const std::string& key) -> std::optional<std::string>;
    auto set(const std::string& key, const std::string& value) -> bool;
    auto unset(const std::string& key) -> void;

    auto which(const std::string& exe, const std::string& override_path = "") -> fs::u8path;
    auto which(const std::string& exe, const std::vector<fs::u8path>& search_paths) -> fs::u8path;
    auto copy() -> std::map<std::string, std::string>;
    auto platform() -> std::string;
    auto home_directory() -> fs::u8path;
    auto user_config_dir() -> fs::u8path;
    auto user_data_dir() -> fs::u8path;
    auto user_cache_dir() -> fs::u8path;

    auto expand_user(const fs::u8path& path) -> fs::u8path;
    auto shrink_user(const fs::u8path& path) -> fs::u8path;

    /********************
     *  Implementation  *
     ********************/

    constexpr auto pathsep() -> const char*
    {
        if (util::on_win)
        {
            return ";";
        }
        else
        {
            return ":";
        }
    }

}
#endif
