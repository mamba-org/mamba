// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PACKAGE_HANDLING_HPP
#define MAMBA_CORE_PACKAGE_HANDLING_HPP

#include <string>
#include <system_error>
#include <vector>

#include "mamba_fs.hpp"

namespace mamba
{
    enum compression_algorithm
    {
        bzip2,
        zip,
        zstd
    };

    void create_archive(const fs::path& directory,
                        const fs::path& destination,
                        compression_algorithm,
                        int compression_level);
    void create_package(const fs::path& directory, const fs::path& out_file, int compression_level);

    void extract_archive(const fs::path& file, const fs::path& destination);
    void extract_conda(const fs::path& file,
                       const fs::path& dest_dir,
                       const std::vector<std::string>& parts = { "info", "pkg" });
    void extract(const fs::path& file, const fs::path& destination);
    fs::path extract(const fs::path& file);
    bool transmute(const fs::path& pkg_file, const fs::path& target, int compression_level);
    bool validate(const fs::path& pkg_folder);
}  // namespace mamba

#endif  // MAMBA_PACKAGE_HANDLING_HPP
