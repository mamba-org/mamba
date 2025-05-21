// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#pragma once

#include <string>

#include "mamba/fs/filesystem.hpp"

// TODO: having a file for this single structure is a bit of
// overkill; this should be refactored when we have more structures
// like this (i.e. parameters structures with no dependency on other
// parts of mamba)
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

    struct ThreadsParams
    {
        std::size_t download_threads{ 5 };
        int extract_threads{ 0 };
    };
}
