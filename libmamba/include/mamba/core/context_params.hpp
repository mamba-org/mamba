// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#pragma once

#include <string>
#include <vector>

#include "mamba/fs/filesystem.hpp"

// TODO move these structures back to Context.hpp or rename this file
// with a better name when the Context is fully refactored.
namespace mamba
{
    struct CommandParams
    {
        std::string caller_version{ "" };
        std::string conda_version{ "3.8.0" };
        std::string current_command{ "mamba" };
        /** Is the Context used in a mamba or mamba executable (instead of a lib). */
        bool is_mamba_exe{ false };
    };

    struct PrefixParams
    {
        fs::u8path target_prefix;
        fs::u8path root_prefix;
        fs::u8path conda_prefix;
        fs::u8path relocate_prefix;
    };

    struct LinkParams
    {
        bool allow_softlinks = false;
        bool always_copy = false;
        bool always_softlink = false;
        bool compile_pyc = true;
    };

    struct ThreadsParams
    {
        std::size_t download_threads{ 5 };
        int extract_threads{ 0 };
    };

    struct TransactionParams
    {
        bool is_mamba_exe;
        bool json_output;
        int verbosity;
        bool shortcuts;
        std::vector<fs::u8path> envs_dirs;
        std::string platform;

        PrefixParams prefix_params;
        LinkParams link_params;
        ThreadsParams threads_params;
    };
}
