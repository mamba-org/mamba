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
}
