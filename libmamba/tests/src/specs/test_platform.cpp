// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/platform.hpp"

using namespace mamba::specs;

TEST_SUITE("platform")
{
    TEST_CASE("name")
    {
        CHECK_EQ(platform_name(Platform::linux_riscv32), "linux-riscv32");
        CHECK_EQ(platform_name(Platform::osx_arm64), "osx-arm64");
        CHECK_EQ(platform_name(Platform::win_64), "win-64");
    }

    TEST_CASE("parse")
    {
        CHECK_EQ(platform_parse("linux-armv6l"), Platform::linux_armv6l);
        CHECK_EQ(platform_parse(" win-32 "), Platform::win_32);
        CHECK_EQ(platform_parse(" OSX-64"), Platform::osx_64);
        CHECK_EQ(platform_parse("linus-46"), std::nullopt);
    }
}
