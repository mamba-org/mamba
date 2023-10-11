// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include "mamba/util/cow.hpp"

using namespace mamba::util;

TEST_SUITE("util::cow")
{
    TEST_CASE("string_cow")
    {
        SUBCASE("Borrow from view")
        {
            const std::string_view str = "Hello world!";

            auto c = string_cow(str);
            CHECK(c.is_borrowed());
            CHECK_FALSE(c.is_owned());

            CHECK_EQ(c.view(), str);
            auto val = c.value();
            CHECK(std::is_same_v<decltype(val), std::string>);
            CHECK_EQ(val, str);
        }

        SUBCASE("Borrow from value")
        {
            const std::string str = "Hello world!";

            auto c = string_cow(str);
            CHECK(c.is_borrowed());
            CHECK_FALSE(c.is_owned());

            CHECK_EQ(c.view(), str);
            auto val = c.value();
            CHECK(std::is_same_v<decltype(val), std::string>);
            CHECK_EQ(val, str);
        }

        SUBCASE("Owned from value")
        {
            std::string_view str = "Hello world!";

            auto c = string_cow(std::string(str));
            CHECK_FALSE(c.is_borrowed());
            CHECK(c.is_owned());

            CHECK_EQ(c.view(), str);
            auto val = c.value();
            CHECK(std::is_same_v<decltype(val), std::string>);
            CHECK_EQ(val, str);
        }

        SUBCASE("make_owned")
        {
            std::string_view str = "Hello world!";

            SUBCASE("from view")
            {
                auto c1 = string_cow::make_owned(str);
                CHECK_FALSE(c1.is_borrowed());
                CHECK(c1.is_owned());
            }

            SUBCASE("from value")
            {
                auto c2 = string_cow::make_owned(std::string(str));
                CHECK_FALSE(c2.is_borrowed());
                CHECK(c2.is_owned());
            }
        }

        SUBCASE("make_borrowed")
        {
            const std::string str = "Hello world!";

            SUBCASE("from value")
            {
                auto c1 = string_cow::make_borrowed(str);
                CHECK(c1.is_borrowed());
                CHECK_FALSE(c1.is_owned());
            }

            SUBCASE("from view")
            {
                auto c2 = string_cow::make_borrowed(std::string_view(str));
                CHECK(c2.is_borrowed());
                CHECK_FALSE(c2.is_owned());
            }
        }
    }
}
