// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PINNING_HPP
#define MAMBA_PINNING_HPP

#include "prefix_data.hpp"

#include <vector>
#include <string>


namespace mamba
{
    std::string python_pin(const PrefixData& prefix_data, const std::vector<std::string>& specs);

    std::vector<std::string> file_pins(const fs::path& file);
}

#endif
