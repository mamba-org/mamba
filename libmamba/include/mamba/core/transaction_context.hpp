// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_TRANSACTION_CONTEXT
#define MAMBA_CORE_TRANSACTION_CONTEXT

#include <string>

#include "context.hpp"
#include "mamba_fs.hpp"

namespace mamba
{
    std::string compute_short_python_version(const std::string& long_version);
    // supply short python version, e.g. 2.7, 3.5...
    fs::path get_python_short_path(const std::string& python_version);
    fs::path get_python_site_packages_short_path(const std::string& python_version);
    fs::path get_bin_directory_short_path();
    fs::path get_python_noarch_target_path(const std::string& source_short_path,
                                           const fs::path& target_site_packages_short_path);

    class TransactionContext
    {
    public:
        TransactionContext() = default;
        TransactionContext(const fs::path& prefix, const std::string& py_version);

        bool has_python;
        fs::path target_prefix;
        fs::path site_packages_path;
        fs::path python_path;
        std::string python_version;
        std::string short_python_version;
        bool allow_softlinks = false;
        bool always_copy = false;
        bool always_softlink = false;
    };
}  // namespace mamba

#endif
