// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UPDATE_HPP
#define MAMBA_UPDATE_HPP

#include "mamba/mamba_fs.hpp"

#include <string>
#include <vector>


namespace mamba
{
    void update(const std::vector<std::string>& specs, const fs::path& prefix = "");
}

#endif
