// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBA_VERSION_HPP
#define LIBMAMBA_VERSION_HPP

#include <array>
#include <string>

#define LIBMAMBA_VERSION_MAJOR 0
#define LIBMAMBA_VERSION_MINOR 18
#define LIBMAMBA_VERSION_PATCH 0

// Binary version
#define LIBMAMBA_BINARY_CURRENT 1
#define LIBMAMBA_BINARY_REVISION 0
#define LIBMAMBA_BINARY_AGE 0

#define LIBMAMBA_VERSION                                                                           \
    (LIBMAMBA_VERSION_MAJOR * 10000 + LIBMAMBA_VERSION_MINOR * 100 + LIBMAMBA_VERSION_PATCH)
#define LIBMAMBA_VERSION_STRING "0.18.0"

namespace mamba
{
    std::string version();

    std::array<int, 3> version_arr();
}

#endif
