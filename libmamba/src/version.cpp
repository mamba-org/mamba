// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/version.hpp"


const char libmamba_version[] = LIBMAMBA_VERSION_STRING;
int libmamba_version_major = LIBMAMBA_VERSION_MAJOR;
int libmamba_version_minor = LIBMAMBA_VERSION_MINOR;
int libmamba_version_patch = LIBMAMBA_VERSION_PATCH;

std::string
version()
{
    return LIBMAMBA_VERSION_STRING;
}
