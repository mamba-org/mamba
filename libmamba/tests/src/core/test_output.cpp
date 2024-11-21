// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"

#include "mambatests.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("no_progress_bars")
        {
            mambatests::context().graphics_params.no_progress_bars = true;
            auto proxy = Console::instance().add_progress_bar("conda-forge");
            REQUIRE_FALSE(proxy.defined());
            REQUIRE_FALSE(proxy);

            mambatests::context().graphics_params.no_progress_bars = false;
            proxy = Console::instance().add_progress_bar("conda-forge");
            REQUIRE(proxy.defined());
            REQUIRE(proxy);
        }
    }
}  // namespace mamba
