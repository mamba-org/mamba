// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_TRANSACTION_CONTEXT
#define MAMBA_CORE_TRANSACTION_CONTEXT

#include <string>

#include <reproc++/reproc.hpp>

#include "mamba/core/context_params.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/match_spec.hpp"

namespace mamba
{
    std::string compute_short_python_version(const std::string& long_version);
    // supply short python version, e.g. 2.7, 3.5...
    fs::u8path get_python_short_path(const std::string& python_version);
    fs::u8path get_python_site_packages_short_path(const std::string& python_version);
    fs::u8path get_bin_directory_short_path();
    fs::u8path get_python_noarch_target_path(
        const std::string& source_short_path,
        const fs::u8path& target_site_packages_short_path
    );

    class TransactionContext
    {
    public:

        struct PythonParams
        {
            bool has_python = false;
            std::string python_version;
            std::string old_python_version;
            std::string short_python_version;
            fs::u8path python_path;
            fs::u8path site_packages_path;
        };

        TransactionContext(
            TransactionParams transaction_params,
            std::pair<std::string, std::string> py_versions,
            std::string python_site_packages_path,
            std::vector<specs::MatchSpec> requested_specs
        );

        ~TransactionContext();

        TransactionContext(TransactionContext&&) = default;
        TransactionContext& operator=(TransactionContext&&) = default;

        bool try_pyc_compilation(const std::vector<fs::u8path>& py_files);
        void wait_for_pyc_compilation();

        const TransactionParams& transaction_params() const;
        const PrefixParams& prefix_params() const;
        const LinkParams& link_params() const;
        const PythonParams& python_params() const;

        const std::vector<specs::MatchSpec>& requested_specs() const;

    private:

        bool start_pyc_compilation_process();

        TransactionParams m_transaction_params;
        PythonParams m_python_params;
        std::vector<specs::MatchSpec> m_requested_specs;

        std::unique_ptr<reproc::process> m_pyc_process = nullptr;
        std::unique_ptr<TemporaryFile> m_pyc_script_file = nullptr;
        std::unique_ptr<TemporaryFile> m_pyc_compileall = nullptr;
    };
}  // namespace mamba

#endif
