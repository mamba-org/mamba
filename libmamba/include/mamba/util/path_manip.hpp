// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_PATH_MANIP_HPP
#define MAMBA_UTIL_PATH_MANIP_HPP

#include <string>
#include <string_view>

namespace mamba::util
{
    inline static constexpr char preferred_path_separator_posix = '/';
    inline static constexpr char preferred_path_separator_win = '\\';

    auto path_win_to_posix(std::string path) -> std::string;
}
#endif
