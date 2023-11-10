// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/util/environment.hpp"

using namespace mamba::util;

TEST_SUITE("util::environment")
{
    TEST_CASE("get_env")
    {
        CHECK_FALSE(get_env("VAR_THAT_DOES_NOT_EXIST_XYZ").has_value());
        CHECK(get_env("PATH").has_value());
    }

    TEST_CASE("set_env")
    {
        set_env("VAR_THAT_DOES_NOT_EXIST_XYZ", "VALUE");
        CHECK_EQ(get_env("VAR_THAT_DOES_NOT_EXIST_XYZ"), "VALUE");
        set_env(u8"VAR_私のにほんごわへたです", u8"😀");
        CHECK_EQ(get_env(u8"VAR_私のにほんごわへたです"), u8"😀");
        set_env(u8"VAR_私のにほんごわへたです", u8"hello");
        CHECK_EQ(get_env(u8"VAR_私のにほんごわへたです"), u8"hello");
    }

    TEST_CASE("unset_env")
    {
        CHECK_FALSE(get_env("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        unset_env("VAR_THAT_DOES_NOT_EXIST_ZZZ");
        CHECK_FALSE(get_env("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        set_env("VAR_THAT_DOES_NOT_EXIST_ZZZ", "VALUE");
        CHECK(get_env("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
        unset_env("VAR_THAT_DOES_NOT_EXIST_ZZZ");
        CHECK_FALSE(get_env("VAR_THAT_DOES_NOT_EXIST_ZZZ").has_value());
    }

    TEST_CASE("get_env_map")
    {
        auto environ = get_env_map();
        CHECK_GT(environ.size(), 0);
        CHECK_EQ(environ.count("VAR_THAT_MUST_NOT_EXIST_XYZ"), 0);
        CHECK_EQ(environ.count("PATH"), 1);

        set_env(u8"VAR_私のにほHelloわへたです", u8"😀");
        environ = get_env_map();
        CHECK_EQ(environ.at(u8"VAR_私のにほHelloわへたです"), u8"😀");
    }
}
