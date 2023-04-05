// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    TEST_SUITE("output")
    {
        TEST_CASE("no_progress_bars")
        {
            Context::instance().no_progress_bars = true;
            auto proxy = Console::instance().add_progress_bar("conda-forge");
            CHECK_FALSE(proxy.defined());
            CHECK_FALSE(proxy);

            Context::instance().no_progress_bars = false;
            proxy = Console::instance().add_progress_bar("conda-forge");
            CHECK(proxy.defined());
            CHECK(proxy);
        }
    }
}  // namespace mamba
