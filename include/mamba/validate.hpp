// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATE_HPP
#define MAMBA_VALIDATE_HPP

#include <string>

#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

namespace validate
{
    std::string sha256sum(const std::string& path);
    std::string md5sum(const std::string& path);
    bool sha256(const std::string& path, const std::string& validation);
    bool md5(const std::string& path, const std::string& validation);
    bool file_size(const fs::path& path, std::uintmax_t validation);
}  // namespace validate

#endif  // MAMBA_VALIDATE_HPP
