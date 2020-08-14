// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PACKAGE_HANDLING_HPP
#define MAMBA_PACKAGE_HANDLING_HPP

#include <string>
#include <system_error>
#include <vector>

#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

namespace mamba
{
    void extract_archive(const fs::path& file, const fs::path& destination);
    void extract_conda(const fs::path& file,
                       const fs::path& dest_dir,
                       const std::vector<std::string>& parts = { "info", "pkg" });
    void split_package_extension(const std::string& file,
                                 std::string& name,
                                 std::string& extension);
    fs::path strip_package_extension(const std::string& file);
    fs::path extract(const fs::path& file);
}  // namespace mamba

#endif  // MAMBA_PACKAGE_HANDLING_HPP
