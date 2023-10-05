// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_URL_HPP
#define MAMBA_CORE_URL_HPP

#include "mamba/core/fsutil.hpp"
#include "mamba/fs/filesystem.hpp"

namespace mamba
{

#ifndef MAMBA_TEST_DATA_DIR
#error "MAMBA_TEST_DATA_DIR must be defined pointing to test data"
#endif
    inline static const fs::u8path test_data_dir = MAMBA_TEST_DATA_DIR;

}

#endif
