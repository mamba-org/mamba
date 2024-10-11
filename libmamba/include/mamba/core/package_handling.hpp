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

#include "mamba/fs/filesystem.hpp"

namespace mamba
{
    struct ValidationParams;
    class Context;

    // Determine the kind of command line to run to extract subprocesses.
    enum class extract_subproc_mode
    {
        /** An external binary packaged with `libmamba` to launch as a subprocess. */
        mamba_package,
        /** The mamba or micromamba executable calling itself. */
        mamba_exe,
    };

    struct ExtractOptions
    {
        bool sparse = false;
        extract_subproc_mode subproc_mode;
        static ExtractOptions from_context(const Context&);
    };

    enum compression_algorithm
    {
        none,
        bzip2,
        zip,
        zstd
    };

    void create_archive(
        const fs::u8path& directory,
        const fs::u8path& destination,
        compression_algorithm,
        int compression_level,
        int compression_threads,
        bool (*filter)(const fs::u8path&)
    );
    void create_package(
        const fs::u8path& directory,
        const fs::u8path& out_file,
        int compression_threads,
        int compression_level
    );

    void
    extract_archive(const fs::u8path& file, const fs::u8path& destination, const ExtractOptions& options);
    void extract_conda(
        const fs::u8path& file,
        const fs::u8path& dest_dir,
        const ExtractOptions& options,
        const std::vector<std::string>& parts = { "info", "pkg" }
    );
    void
    extract(const fs::u8path& file, const fs::u8path& destination, const ExtractOptions& options);
    fs::u8path extract(const fs::u8path& file, const ExtractOptions& options);


    void
    extract_subproc(const fs::u8path& file, const fs::u8path& dest, const ExtractOptions& options);

    bool transmute(
        const fs::u8path& pkg_file,
        const fs::u8path& target,
        int compression_level,
        int compression_threads,
        const ExtractOptions& options
    );

    bool validate(const fs::u8path& pkg_folder, const ValidationParams& params);

}  // namespace mamba

#endif  // MAMBA_PACKAGE_HANDLING_HPP
