// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_API_SHELL_HPP
#define MAMBA_API_SHELL_HPP

#include <string>
#include <vector>

#include "mamba/core/mamba_fs.hpp"


namespace mamba
{
    void detect_shell(std::string& shell_type);

    void shell(
        const std::string& action,
        std::string& shell_type,
        const std::string& prefix = "",
        bool stack = false
    );
}

#endif
