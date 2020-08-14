// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#define MAMBA_VERSION_MAJOR 0
#define MAMBA_VERSION_MINOR 4
#define MAMBA_VERSION_PATCH 4

// Binary version
#define MAMBA_BINARY_CURRENT 1
#define MAMBA_BINARY_REVISION 0
#define MAMBA_BINARY_AGE 0

#define MAMBA_VERSION                                                                              \
    (MAMBA_VERSION_MAJOR * 10000 + MAMBA_VERSION_MINOR * 100 + MAMBA_VERSION_PATCH)
#define MAMBA_VERSION_STRING "0.4.4"

extern const char mamba_version[];
extern int mamba_version_major;
extern int mamba_version_minor;
extern int mamba_version_patch;
