// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/util/environment.hpp"

using namespace mamba;

TEST_SUITE("util::environment")
{
    TEST_CASE("getenv")
    {
        CHECK_FALSE(util::getenv("VAR_THAT_DOES_NOT_EXIST_XYZ").has_value());
        CHECK(util::getenv("PATH").has_value());
    }

    TEST_CASE("setenv")
    {
        util::setenv("VAR_THAT_DOES_NOT_EXIST_XYZ", "VALUE");
        CHECK_EQ(util::getenv("VAR_THAT_DOES_NOT_EXIST_XYZ"), "VALUE");
        util::setenv(u8"VAR_私のにほんごわへたです", u8"😀");
        CHECK_EQ(util::getenv(u8"VAR_私のにほんごわへたです"), u8"😀");
        util::setenv(u8"VAR_私のにほんごわへたです", u8"hello");
        CHECK_EQ(util::getenv(u8"VAR_私のにほんごわへたです"), u8"hello");
    }

    TEST_CASE("unsetenv")
    {
        CHECK_FALSE(util::getenv("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        util::unsetenv("VAR_THAT_DOES_NOT_EXIST_ZZZ");
        CHECK_FALSE(util::getenv("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        util::setenv("VAR_THAT_DOES_NOT_EXIST_ZZZ", "VALUE");
        CHECK(util::getenv("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        util::unsetenv("VAR_THAT_DOES_NOT_EXIST_ZZZ");
        CHECK_FALSE(util::getenv("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
    }
}
