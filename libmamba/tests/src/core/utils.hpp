// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef LIBMAMBA_TESTS_CORE_UTILS_HPP
#define LIBMAMBA_TESTS_CORE_UTILS_HPP

#include <string>
#include <vector>

#include "mamba/fs/filesystem.hpp"

namespace mambatests
{
    auto read_explicit_urls(const mamba::fs::u8path& path) -> std::vector<std::string>;
}

#endif
