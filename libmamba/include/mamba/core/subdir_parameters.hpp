// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_SUBDIR_PARAMETERS_HPP
#define MAMBA_CORE_SUBDIR_PARAMETERS_HPP

#include <optional>

namespace mamba
{
    struct SubdirParams
    {
        /**
         * Repodata cache time to live in seconds.
         *
         * If not specified, then it is read from server headers.
         */
        std::optional<std::size_t> local_repodata_ttl_s = std::nullopt;
        bool offline = false;
        /** Force the use of zst for this subdir without checking. */
        bool repodata_force_use_zst = false;
    };

    struct SubdirDownloadParams
    {
        bool offline = false;
        /** Make a request to check the use of zst compression format. */
        bool repodata_check_zst = true;
    };
}

#endif
