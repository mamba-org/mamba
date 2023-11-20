// Copyright (c) 2019-2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_ENVIRONMENT_HPP
#define MAMBA_CORE_ENVIRONMENT_HPP

#include <string_view>

#include "mamba/fs/filesystem.hpp"

namespace mamba::env
{
    [[nodiscard]] auto which(std::string_view exe, std::string_view override_path = "") -> fs::u8path;

    template <typename Iter>
    [[nodiscard]] auto which_in(std::string_view exe, Iter search_path_first, Iter search_path_last)
        -> fs::u8path;

    template <typename Range>
    [[nodiscard]] auto which_in(std::string_view exe, const Range& search_paths) -> fs::u8path;

    /********************
     *  Implementaiton  *
     ********************/

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
