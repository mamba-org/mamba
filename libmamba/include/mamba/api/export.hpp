// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_EXPORT_HPP
#define MAMBA_API_EXPORT_HPP

#include "mamba/api/configuration.hpp"

namespace mamba
{
    class Configuration;

    void export_environment(Configuration& config);
}

#endif
