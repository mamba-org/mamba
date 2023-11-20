// Copyright (c) 2019-2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_ENVIRONMENT_HPP
#define MAMBA_CORE_ENVIRONMENT_HPP

#include <string_view>

#include "mamba/fs/filesystem.hpp"
#include "mamba/util/build.hpp"

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
        [[nodiscard]] auto
        which_in_impl(const fs::u8path& exe, const fs::u8path& dir, const fs::u8path& extension)
            -> fs::u8path;
    }


    template <typename Iter>
    auto which_in(std::string_view exe, Iter search_path_first, Iter search_path_last) -> fs::u8path
    {
        const auto extension = []() -> fs::u8path
        {
            if (util::on_win)
            {
                return ".exe";
            }
            return "";
        }();

        for (; search_path_first != search_path_last; ++search_path_first)
        {
            if (auto p = detail::which_in_impl(exe, *search_path_first, extension); !p.empty())
            {
                return p;
            }
        }
        return "";
    }

    template <typename Range>
    auto which_in(std::string_view exe, const Range& search_paths) -> fs::u8path
    {
        return which_in(exe, search_paths.cbegin(), search_paths.cend());
    }
}
#endif
