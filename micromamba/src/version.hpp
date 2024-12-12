// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef UMAMBA_VERSION_HPP
#define UMAMBA_VERSION_HPP

#include <array>
#include <string>

#define UMAMBA_VERSION_MAJOR 2
#define UMAMBA_VERSION_MINOR 0
#define UMAMBA_VERSION_PATCH 5
#define UMAMBA_VERSION_IS_PRERELEASE 0
#if UMAMBA_VERSION_IS_PRERELEASE == 1
#define UMAMBA_VERSION_PRERELEASE_NAME ""
#endif

#define UMAMBA_VERSION_STRING "2.0.5"
#define UMAMBA_VERSION                                                                             \
    (UMAMBA_VERSION_MAJOR * 10000 + UMAMBA_VERSION_MINOR * 100 + UMAMBA_VERSION_PATCH)

// Binary version
#define UMAMBA_BINARY_CURRENT 1
#define UMAMBA_BINARY_REVISION 0
#define UMAMBA_BINARY_AGE 0

namespace umamba
{
    std::string version();

    [[deprecated("will be replaced in a future minor version")]]
    std::array<int, 3> version_arr();

}

#endif
