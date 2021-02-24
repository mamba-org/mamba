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
    void pin_python_spec(const PrefixData& prefix_data, std::vector<std::string>& specs);

    void pin_config_specs(const std::vector<std::string>& config_specs,
                          std::vector<std::string>& specs);

    void pin_file_specs(const fs::path& file_specs, std::vector<std::string>& specs);
}

#endif
