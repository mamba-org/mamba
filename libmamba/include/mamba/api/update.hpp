// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_UPDATE_HPP
#define MAMBA_API_UPDATE_HPP

namespace mamba
{
    class Configuration;

    void update(
        Configuration& config,
        bool update_all = false,
        bool prune_deps = false,
        bool remove_not_specified = false
    );
}

#endif
