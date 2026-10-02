// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATION_TOOLS_HPP
#define MAMBA_VALIDATION_TOOLS_HPP

#include <array>
#include <cstddef>
#include <string>

namespace mamba::fs
{
    class u8path;
}

namespace mamba::validation
{
    [[nodiscard]] auto sha256sum(const fs::u8path& path) -> std::string;

    [[nodiscard]] auto md5sum(const fs::u8path& path) -> std::string;

    auto file_size(const fs::u8path& path, std::uintmax_t validation) -> bool;
}
#endif
