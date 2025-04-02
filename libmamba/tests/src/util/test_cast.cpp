// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cmath>
#include <limits>
#include <utility>

#include <catch2/catch_all.hpp>

#include "mamba/util/cast.hpp"

using namespace mamba::util;

namespace
{
    template <typename From, typename To>
    void check_exact_num_cast_widen()
    {
        static constexpr auto from_lowest = std::numeric_limits<From>::lowest();
        static constexpr auto from_max = std::numeric_limits<From>::max();

        REQUIRE(safe_num_cast<To>(From(0)) == To(0));
        REQUIRE(safe_num_cast<To>(From(1)) == To(1));
        REQUIRE(safe_num_cast<To>(from_lowest) == static_cast<To>(from_lowest));
        REQUIRE(safe_num_cast<To>(from_max) == static_cast<To>(from_max));
    }

    TEST_CASE("Exact num cast widen - integers")
    {
        check_exact_num_cast_widen<char, int>();
        check_exact_num_cast_widen<unsigned char, int>();
        check_exact_num_cast_widen<unsigned char, unsigned int>();
        check_exact_num_cast_widen<int, long long int>();
        check_exact_num_cast_widen<unsigned int, long long int>();
        check_exact_num_cast_widen<unsigned int, unsigned long long int>();
    }

    TEST_CASE("Exact num cast widen - floats")
    {
        check_exact_num_cast_widen<float, double>();
    }

    TEST_CASE("Exact num cast widen - mixed")
    {
        check_exact_num_cast_widen<char, float>();
        check_exact_num_cast_widen<unsigned char, float>();
        check_exact_num_cast_widen<int, double>();
        check_exact_num_cast_widen<unsigned int, double>();
    }

    template <typename From, typename To>
    void check_exact_num_cast_narrow()
    {
        REQUIRE(safe_num_cast<To>(From(0)) == To(0));
        REQUIRE(safe_num_cast<To>(From(1)) == To(1));
    }

    TEST_CASE("Exact num cast narrow - integers")
    {
        check_exact_num_cast_narrow<int, char>();
        check_exact_num_cast_narrow<unsigned int, unsigned char>();
        check_exact_num_cast_narrow<long long int, int>();
        check_exact_num_cast_narrow<unsigned long long int, unsigned int>();
    }

    TEST_CASE("Exact num cast narrow - floats")
    {
        check_exact_num_cast_narrow<double, float>();
    }

    template <typename From, typename To>
    void check_exact_num_cast_overflow()
    {
        static constexpr auto from_lowest = std::numeric_limits<From>::lowest();
        REQUIRE_THROWS_AS(safe_num_cast<To>(from_lowest), std::overflow_error);
    }

    TEST_CASE("Exact num cast overflow - integers")
    {
        check_exact_num_cast_overflow<char, unsigned char>();
        check_exact_num_cast_overflow<char, unsigned int>();
        check_exact_num_cast_overflow<int, char>();
        check_exact_num_cast_overflow<int, unsigned long long int>();
    }

    TEST_CASE("Exact num cast overflow - floats")
    {
        check_exact_num_cast_overflow<double, float>();
    }

    TEST_CASE("Exact num cast overflow - mixed")
    {
        check_exact_num_cast_overflow<double, int>();
        check_exact_num_cast_overflow<float, char>();
    }

    TEST_CASE("precision")
    {
        REQUIRE_THROWS_AS(safe_num_cast<int>(1.1), std::runtime_error);
        REQUIRE_THROWS_AS(safe_num_cast<float>(std::nextafter(double(1), 2)), std::runtime_error);
    }
}
