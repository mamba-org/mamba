// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SHELL_HPP
#define MAMBA_SHELL_HPP

#include "mamba/mamba_fs.hpp"

#include <string>
#include <vector>


namespace mamba
{
    void shell(const std::string& action,
               const std::string& shell_type,
               const fs::path& prefix = "");

    namespace detail
    {
    }
}

#endif
