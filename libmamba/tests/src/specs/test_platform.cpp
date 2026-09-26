// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/platform.hpp"

using namespace mamba::specs;

namespace
{
    TEST_CASE("KnownPlatform")
    {
        SECTION("name")
        {
            REQUIRE(platform_name(KnownPlatform::linux_riscv32) == "linux-riscv32");
            REQUIRE(platform_name(KnownPlatform::linux_loongarch64) == "linux-loongarch64");
            REQUIRE(platform_name(KnownPlatform::osx_arm64) == "osx-arm64");
            REQUIRE(platform_name(KnownPlatform::win_64) == "win-64");
        }

        SECTION("parse")
        {
            REQUIRE(platform_parse("linux-armv6l") == KnownPlatform::linux_armv6l);
            REQUIRE(platform_parse("linux-loongarch64") == KnownPlatform::linux_loongarch64);
            REQUIRE(platform_parse(" win-32 ") == KnownPlatform::win_32);
            REQUIRE(platform_parse(" OSX-64") == KnownPlatform::osx_64);
            REQUIRE(platform_parse("linus-46") == std::nullopt);
        }

        SECTION("known_platform")
        {
            static constexpr decltype(known_platform_names()) expected{
                "noarch",       "linux-32",      "linux-64",      "linux-armv6l",
                "linux-armv7l", "linux-aarch64", "linux-ppc64le", "linux-ppc64",
                "linux-s390x",  "linux-riscv32", "linux-riscv64", "linux-loongarch64",
                "osx-64",       "osx-arm64",     "win-32",        "win-64",
                "win-arm64",    "zos-z",

            };
            REQUIRE(expected == known_platform_names());
        }
    }

    TEST_CASE("platform_is_xxx")
    {
        SECTION("KnownPlatform")
        {
            // Making sure no-one forgot to add the platform with a specific OS
            for (auto plat : known_platforms())
            {
                auto check = platform_is_linux(plat)             //
                             || platform_is_osx(plat)            //
                             || platform_is_win(plat)            //
                             || (plat == KnownPlatform::noarch)  //
                             || (plat == KnownPlatform::zos_z);
                REQUIRE(check);
            }
        }

        SECTION("DynamicPlatform")
        {
            REQUIRE_FALSE(platform_is_linux("win-64"));
            REQUIRE_FALSE(platform_is_linux("osx-64"));
            REQUIRE(platform_is_linux("linux-64"));

            REQUIRE_FALSE(platform_is_osx("win-64"));
            REQUIRE(platform_is_osx("osx-64"));
            REQUIRE_FALSE(platform_is_osx("linux-64"));

            REQUIRE(platform_is_win("win-64"));
            REQUIRE_FALSE(platform_is_win("osx-64"));
            REQUIRE_FALSE(platform_is_win("linux-64"));
        }
    }

    TEST_CASE("NoArch")
    {
        SECTION("name")
        {
            REQUIRE(noarch_name(NoArchType::No) == "no");
            REQUIRE(noarch_name(NoArchType::Generic) == "generic");
            REQUIRE(noarch_name(NoArchType::Python) == "python");
        }

        SECTION("parse")
        {
            REQUIRE(noarch_parse("") == std::nullopt);
            REQUIRE(noarch_parse(" Python ") == NoArchType::Python);
            REQUIRE(noarch_parse(" geNeric") == NoArchType::Generic);
            REQUIRE(noarch_parse("Nothing we know") == std::nullopt);
        }

        SECTION("known_noarch")
        {
            static constexpr decltype(known_noarch_names()) expected{ "no", "generic", "python" };
            REQUIRE(expected == known_noarch_names());
        }
    }
}
