// Copyright (c) 2025, Cppreference.com
//
// Distributed under the terms of the Copyright/CC-BY-SA License.
//
// The full license can be found at the address
// https://en.cppreference.com/w/Cppreference:Copyright/CC-BY-SA

#include <catch2/catch_all.hpp>

#include "mamba/util/charconv.hpp"

using namespace mamba::util;

namespace
{
    TEST_CASE("constexpr_from_chars works for valid input", "[mamba::util]")
    {
        SECTION("Basic parsing")
        {
            constexpr const char* input = "12345";
            unsigned value = 0;
            auto res = constexpr_from_chars(input, input + 5u, value);
            REQUIRE(res.ec == std::errc{});
            REQUIRE(res.ptr == input + 5);
            REQUIRE(value == 12345u);
        }

        SECTION("Empty input")
        {
            constexpr const char* input = "";
            std::size_t value = 0;
            auto res = constexpr_from_chars(input, input, value);
            REQUIRE(res.ec == std::errc::invalid_argument);
            REQUIRE(res.ptr == input);
        }

        SECTION("Non-digit character")
        {
            constexpr const char* input = "123a";
            unsigned value = 0;
            auto res = constexpr_from_chars(input, input + 4, value);
            REQUIRE(res.ec == std::errc{});
            REQUIRE(res.ptr == input + 3);
            REQUIRE(value == 123u);
        }

        SECTION("No digits at all")
        {
            constexpr const char* input = "abc";
            unsigned value = 0;
            auto res = constexpr_from_chars(input, input + 3, value);
            REQUIRE(res.ec == std::errc::invalid_argument);
            REQUIRE(res.ptr == input);
        }

        SECTION("Overflow")
        {
            constexpr const char* input = "99999999999999999999";
            std::size_t value = 0;
            auto res = constexpr_from_chars(input, input + 20, value);
            REQUIRE(res.ec == std::errc::result_out_of_range);
        }

        SECTION("Leading zeroes")
        {
            constexpr const char* input = "00042";
            unsigned value = 0;
            auto res = constexpr_from_chars(input, input + 5, value);
            REQUIRE(res.ec == std::errc{});
            REQUIRE(res.ptr == input + 5);
            REQUIRE(value == 42u);
        }
    }
}
