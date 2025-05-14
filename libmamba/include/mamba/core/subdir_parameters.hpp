// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SUBDIR_PARAMETERS_HPP
#define MAMBA_CORE_SUBDIR_PARAMETERS_HPP

#include <cstddef>

namespace mamba
{
    struct SubdirParams
    {
        std::size_t local_repodata_ttl = 1;
        bool offline = false;
        bool use_index_cache = true;
        bool repodata_use_zst = true;
    };
}

#endif
