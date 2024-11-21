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

using WidenTypes = std::tuple<
    // integers
    std::pair<char, int>,
    std::pair<unsigned char, int>,
    std::pair<unsigned char, unsigned int>,
    std::pair<int, long long int>,
    std::pair<unsigned int, long long int>,
    std::pair<unsigned int, unsigned long long int>,
    // floats
    std::pair<float, double>,
    // Mixed
    std::pair<char, float>,
    std::pair<unsigned char, float>,
    std::pair<int, double>,
    std::pair<unsigned int, double>>;

using OverflowLowestTypes = std::tuple<
    // integers
    std::pair<char, unsigned char>,
    std::pair<char, unsigned int>,
    std::pair<int, char>,
    std::pair<int, unsigned long long int>,
    // floats
    std::pair<double, float>,
    // mixed
    std::pair<double, int>,
    std::pair<float, char>>;

namespace
{
    TEST_CASE_TEMPLATE_DEFINE("checked_exact_num_cast_widen", T, cast_widen)
    {
        using From = typename T::first_type;
        using To = typename T::second_type;
        static constexpr auto from_lowest = std::numeric_limits<From>::lowest();
        static constexpr auto from_max = std::numeric_limits<From>::max();

        REQUIRE(safe_num_cast<To>(From(0)) == To(0);
        REQUIRE(safe_num_cast<To>(From(1)) == To(1);
        REQUIRE(safe_num_cast<To>(from_lowest) == static_cast<To>(from_lowest);
        REQUIRE(safe_num_cast<To>(from_max) == static_cast<To>(from_max);
    }

    TEST_CASE_TEMPLATE_APPLY(cast_widen, WidenTypes);

    TEST_CASE_TEMPLATE_DEFINE("checked_exact_num_cast_narrow", T, cast_narrow)
    {
        using From = typename T::second_type;  // inversed
        using To = typename T::first_type;     // inversed
        REQUIRE(safe_num_cast<To>(From(0)) == To(0);
        REQUIRE(safe_num_cast<To>(From(1)) == To(1);
    }

    TEST_CASE_TEMPLATE_APPLY(cast_narrow, WidenTypes);

    TEST_CASE_TEMPLATE_DEFINE("checked_exact_num_cast_overflow", T, cast_overflow)
    {
        using From = typename T::first_type;
        using To = typename T::second_type;
        static constexpr auto from_lowest = std::numeric_limits<From>::lowest();

        CHECK_THROWS_AS(safe_num_cast<To>(from_lowest), std::overflow_error);
    }

    TEST_CASE_TEMPLATE_APPLY(cast_overflow, OverflowLowestTypes);

    TEST_CASE("precision")
    {
        CHECK_THROWS_AS(safe_num_cast<int>(1.1), std::runtime_error);
        CHECK_THROWS_AS(safe_num_cast<float>(std::nextafter(double(1), 2)), std::runtime_error);
    }
}
