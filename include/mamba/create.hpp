// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CREATE_HPP
#define MAMBA_CREATE_HPP

#include "mamba/mamba_fs.hpp"

#include <string>
#include <vector>


namespace mamba
{
    void create(const std::vector<std::string>& specs, const fs::path& prefix = "");
}

#endif
