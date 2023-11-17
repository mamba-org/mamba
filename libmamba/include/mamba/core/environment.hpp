// Copyright (c) 2019-2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_ENVIRONMENT_HPP
#define MAMBA_CORE_ENVIRONMENT_HPP

#include <string>
#include <vector>

#include "mamba/fs/filesystem.hpp"

namespace mamba::env
{
    auto which(const std::string& exe, const std::string& override_path = "") -> fs::u8path;
    auto which(const std::string& exe, const std::vector<fs::u8path>& search_paths) -> fs::u8path;
}
#endif
