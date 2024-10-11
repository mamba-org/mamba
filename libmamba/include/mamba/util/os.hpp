// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_OS_HPP
#define MAMBA_UTIL_OS_HPP

#include <string>

namespace mamba::util
{
    struct OSError
    {
        std::string message = {};
    };
}
#endif
