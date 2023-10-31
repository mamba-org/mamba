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
    TEST_CASE("get_env")
    {
        CHECK_FALSE(util::get_env("VAR_THAT_DOES_NOT_EXIST_XYZ").has_value());
        CHECK(util::get_env("PATH").has_value());
    }

    TEST_CASE("set_env")
    {
        util::set_env("VAR_THAT_DOES_NOT_EXIST_XYZ", "VALUE");
        CHECK_EQ(util::get_env("VAR_THAT_DOES_NOT_EXIST_XYZ"), "VALUE");
        util::set_env(u8"VAR_私のにほんごわへたです", u8"😀");
        CHECK_EQ(util::get_env(u8"VAR_私のにほんごわへたです"), u8"😀");
        util::set_env(u8"VAR_私のにほんごわへたです", u8"hello");
        CHECK_EQ(util::get_env(u8"VAR_私のにほんごわへたです"), u8"hello");
    }

    TEST_CASE("unset_env")
    {
        CHECK_FALSE(util::get_env("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        util::unset_env("VAR_THAT_DOES_NOT_EXIST_ZZZ");
        CHECK_FALSE(util::get_env("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        util::set_env("VAR_THAT_DOES_NOT_EXIST_ZZZ", "VALUE");
        CHECK(util::get_env("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        util::unset_env("VAR_THAT_DOES_NOT_EXIST_ZZZ");
        CHECK_FALSE(util::get_env("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
    }
}
