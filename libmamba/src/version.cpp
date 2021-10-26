// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/version.hpp"


namespace mamba
{
    std::string version()
    {
        return LIBMAMBA_VERSION_STRING;
    }

    int* version_arr()
    {
        static int v[3];
        v[0] = LIBMAMBA_VERSION_MAJOR;
        v[1] = LIBMAMBA_VERSION_MINOR;
        v[2] = LIBMAMBA_VERSION_PATCH;
        return v;
    }

    int version_major()
    {
        return LIBMAMBA_VERSION_MAJOR;
    }

    int version_minor()
    {
        return LIBMAMBA_VERSION_MINOR;
    }

    int version_patch()
    {
        return LIBMAMBA_VERSION_PATCH;
    }
}
