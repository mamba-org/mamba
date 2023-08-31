// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>

#include "mamba/util/path_manip.hpp"

namespace mamba::util
{
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
}
