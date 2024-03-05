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
    TEST_CASE("KnownPlatform")
    {
        SUBCASE("name")
        {
            CHECK_EQ(platform_name(KnownPlatform::linux_riscv32), "linux-riscv32");
            CHECK_EQ(platform_name(KnownPlatform::osx_arm64), "osx-arm64");
            CHECK_EQ(platform_name(KnownPlatform::win_64), "win-64");
        }

        SUBCASE("parse")
        {
            CHECK_EQ(platform_parse("linux-armv6l"), KnownPlatform::linux_armv6l);
            CHECK_EQ(platform_parse(" win-32 "), KnownPlatform::win_32);
            CHECK_EQ(platform_parse(" OSX-64"), KnownPlatform::osx_64);
            CHECK_EQ(platform_parse("linus-46"), std::nullopt);
        }

        SUBCASE("known_platform")
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

    TEST_CASE("NoArch")
    {
        SUBCASE("name")
        {
            CHECK_EQ(noarch_name(NoArchType::No), "no");
            CHECK_EQ(noarch_name(NoArchType::Generic), "generic");
            CHECK_EQ(noarch_name(NoArchType::Python), "python");
        }

        SUBCASE("parse")
        {
            CHECK_EQ(noarch_parse(""), std::nullopt);
            CHECK_EQ(noarch_parse(" Python "), NoArchType::Python);
            CHECK_EQ(noarch_parse(" geNeric"), NoArchType::Generic);
            CHECK_EQ(noarch_parse("Nothing we know"), std::nullopt);
        }

        SUBCASE("known_noarch")
        {
            static constexpr decltype(known_noarch_names()) expected{ "no", "generic", "python" };
            CHECK_EQ(expected, known_noarch_names());
        }
    }
}
