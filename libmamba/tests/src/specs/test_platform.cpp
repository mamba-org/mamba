// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/specs/platform.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::platform")
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

    TEST_CASE("known_platform")
    {
        static constexpr decltype(known_platform_names()) expected{
            "noarch",        "linux-32",      "linux-64",    "linux-armv6l", "linux-armv7l",
            "linux-aarch64", "linux-ppc64le", "linux-ppc64", "linux-s390x",  "linux-riscv32",
            "linux-riscv64", "osx-64",        "osx-arm64",   "win-32",       "win-64",
            "win-arm64",     "zos-z",

        };
        CHECK_EQ(expected, known_platform_names());
    }
}
