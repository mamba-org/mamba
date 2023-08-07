// Copyright (c) 2022, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>

#include <doctest/doctest.h>

#include "mamba/core/environment.hpp"
#include "mamba/core/util.hpp"
#include "mamba/util/string.hpp"

namespace mamba
{
    TEST_SUITE("shell_init")
    {
        TEST_CASE("init")
        {
            // std::cout << rcfile_content("/home/wolfv/miniconda3/", "bash");
        }

        TEST_CASE("bashrc_modifications")
        {
            // modify_rc_file("/home/wolfv/Programs/mamba/test/.bashrc",
            // "/home/wolfv/superconda/", "bash");
        }

        TEST_CASE("expand_user")
        {
            auto expanded = env::expand_user("~/this/is/a/test");
            if (on_linux)
            {
                CHECK(util::starts_with(expanded.string(), "/home/"));
            }
        }
    }
}  // namespace mamba
