// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <doctest/doctest.h>

#include "mamba/specs/url.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::CondaURL")
{
    TEST_CASE("Token")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SUBCASE("https://repo.mamba.pm/folder/file.txt")
        {
            url.set_path("/folder/file.txt");
            CHECK_EQ(url.token(), "");

            CHECK_THROWS_AS(url.set_token("token"), std::invalid_argument);
            CHECK_EQ(url.path(), "/folder/file.txt");

            CHECK_FALSE(url.clear_token());
            CHECK_EQ(url.path(), "/folder/file.txt");
        }

        SUBCASE("https://repo.mamba.pm/t/xy-12345678-1234/conda-forge/linux-64")
        {
            url.set_path("/t/xy-12345678-1234/conda-forge/linux-64");
            CHECK_EQ(url.token(), "xy-12345678-1234");

            SUBCASE("Cannot set invalid token")
            {
                CHECK_THROWS_AS(url.set_token(""), std::invalid_argument);
                CHECK_THROWS_AS(url.set_token("?fds:g"), std::invalid_argument);
                CHECK_EQ(url.token(), "xy-12345678-1234");
                CHECK_EQ(url.path(), "/t/xy-12345678-1234/conda-forge/linux-64");
            }

            SUBCASE("Clear token")
            {
                CHECK(url.clear_token());
                CHECK_EQ(url.token(), "");
                CHECK_EQ(url.path(), "/conda-forge/linux-64");
            }

            SUBCASE("Set token")
            {
                url.set_token("abcd");
                CHECK_EQ(url.token(), "abcd");
                CHECK_EQ(url.path(), "/t/abcd/conda-forge/linux-64");
            }
        }

        SUBCASE("https://repo.mamba.pm/t/xy-12345678-1234-1234-1234-123456789012")
        {
            url.set_path("/t/xy-12345678-1234-1234-1234-123456789012");
            CHECK_EQ(url.token(), "xy-12345678-1234-1234-1234-123456789012");

            url.set_token("abcd");
            CHECK_EQ(url.token(), "abcd");
            CHECK_EQ(url.path(), "/t/abcd");

            CHECK(url.clear_token());
            CHECK_EQ(url.path(), "/");
        }

        SUBCASE("https://repo.mamba.pm/bar/t/xy-12345678-1234-1234-1234-123456789012/")
        {
            url.set_path("/bar/t/xy-12345678-1234-1234-1234-123456789012/");
            CHECK_EQ(url.token(), "xy-12345678-1234-1234-1234-123456789012");

            url.set_token("abcd");
            CHECK_EQ(url.token(), "abcd");
            CHECK_EQ(url.path(), "/bar/t/abcd/");

            CHECK(url.clear_token());
            CHECK_EQ(url.path(), "/bar/");
        }
    }

    TEST_CASE("Platform")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SUBCASE("https://repo.mamba.pm/")
        {
            CHECK_FALSE(url.platform().has_value());
            CHECK_EQ(url.platform_name(), "");

            CHECK_THROWS_AS(url.set_platform(Platform::linux_64), std::invalid_argument);
            CHECK_EQ(url.path(), "/");

            CHECK_FALSE(url.clear_platform());
            CHECK_EQ(url.path(), "/");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge")
        {
            url.set_path("conda-forge");

            CHECK_FALSE(url.platform().has_value());
            CHECK_EQ(url.platform_name(), "");

            CHECK_THROWS_AS(url.set_platform(Platform::linux_64), std::invalid_argument);
            CHECK_EQ(url.path(), "/conda-forge");

            CHECK_FALSE(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/")
        {
            url.set_path("conda-forge/");

            CHECK_FALSE(url.platform().has_value());
            CHECK_EQ(url.platform_name(), "");

            CHECK_THROWS_AS(url.set_platform(Platform::linux_64), std::invalid_argument);
            CHECK_EQ(url.path(), "/conda-forge/");

            CHECK_FALSE(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge/");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/win-64")
        {
            url.set_path("conda-forge/win-64");

            CHECK_EQ(url.platform(), Platform::win_64);
            CHECK_EQ(url.platform_name(), "win-64");

            url.set_platform(Platform::linux_64);
            CHECK_EQ(url.platform(), Platform::linux_64);
            CHECK_EQ(url.path(), "/conda-forge/linux-64");

            CHECK(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/OSX-64/")
        {
            url.set_path("conda-forge/OSX-64");

            CHECK_EQ(url.platform(), Platform::osx_64);
            CHECK_EQ(url.platform_name(), "OSX-64");  // Captialization not changed

            url.set_platform("Win-64");
            CHECK_EQ(url.platform(), Platform::win_64);
            CHECK_EQ(url.path(), "/conda-forge/Win-64");  // Captialization not changed

            CHECK(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2")
        {
            url.set_path("/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2");

            CHECK_EQ(url.platform(), Platform::linux_64);
            CHECK_EQ(url.platform_name(), "linux-64");

            url.set_platform("osx-64");
            CHECK_EQ(url.platform(), Platform::osx_64);
            CHECK_EQ(url.path(), "/conda-forge/osx-64/micromamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_platform());
            CHECK_EQ(url.path(), "/conda-forge/micromamba-1.5.1-0.tar.bz2");
        }
    }

    TEST_CASE("Package")
    {
        CondaURL url{};
        url.set_scheme("https");
        url.set_host("repo.mamba.pm");

        SUBCASE("https://repo.mamba.pm/")
        {
            CHECK_EQ(url.package(), "");

            CHECK_THROWS_AS(url.set_package("not-package/"), std::invalid_argument);
            CHECK_EQ(url.path(), "/");

            CHECK_FALSE(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.package(), "micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.path(), "/micromamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge")
        {
            url.set_path("conda-forge");

            CHECK_EQ(url.package(), "");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.package(), "micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.path(), "/conda-forge/micromamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/")
        {
            url.set_path("conda-forge/");

            CHECK_EQ(url.package(), "");

            url.set_package("micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.package(), "micromamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.path(), "/conda-forge/micromamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/conda-forge");
        }

        SUBCASE("https://repo.mamba.pm/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2")
        {
            url.set_path("/conda-forge/linux-64/micromamba-1.5.1-0.tar.bz2");

            CHECK_EQ(url.package(), "micromamba-1.5.1-0.tar.bz2");

            url.set_package("mamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.package(), "mamba-1.5.1-0.tar.bz2");
            CHECK_EQ(url.path(), "/conda-forge/linux-64/mamba-1.5.1-0.tar.bz2");

            CHECK(url.clear_package());
            CHECK_EQ(url.package(), "");
            CHECK_EQ(url.path(), "/conda-forge/linux-64");
        }
    }
}
