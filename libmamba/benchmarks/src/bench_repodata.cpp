// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cstdlib>
#include <stdexcept>

#include <benchmark/benchmark.h>

#include "mamba/core/mamba_fs.hpp"

namespace
{
    auto repodata_file() -> fs::u8path
    {
        const char* repodata_file_path = std::getenv("MAMBA_REPODATA_JSON");
        if (repodata_file_path == nullptr)
        {
            throw std::runtime_error("Expected MAMBA_REPODATA_JSON variable");
        }
        return { repodata_file_path };
    }
}
