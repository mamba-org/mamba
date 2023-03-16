// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cmath>
#include <limits>
#include <utility>

#include <gtest/gtest.h>

#include "mamba/util/cast.hpp"

using namespace mamba::util;

template <typename T>
struct cast_valid : ::testing::Test
{
    using First = typename T::first_type;
    using Second = typename T::second_type;
};
using WidenTypes = ::testing::Types<
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
TYPED_TEST_SUITE(cast_valid, WidenTypes);

TYPED_TEST(cast_valid, checked_exact_num_cast_widen)
{
    using From = typename TestFixture::First;
    using To = typename TestFixture::Second;
    static constexpr auto from_lowest = std::numeric_limits<From>::lowest();
    static constexpr auto from_max = std::numeric_limits<From>::max();

    EXPECT_EQ(safe_num_cast<To>(From(0)), To(0));
    EXPECT_EQ(safe_num_cast<To>(From(1)), To(1));
    EXPECT_EQ(safe_num_cast<To>(from_lowest), static_cast<To>(from_lowest));
    EXPECT_EQ(safe_num_cast<To>(from_max), static_cast<To>(from_max));
}

TYPED_TEST(cast_valid, checked_exact_num_cast_narrow)
{
    using From = typename TestFixture::Second;  // inversed
    using To = typename TestFixture::First;     // inversed
    EXPECT_EQ(safe_num_cast<To>(From(0)), To(0));
    EXPECT_EQ(safe_num_cast<To>(From(1)), To(1));
}

template <typename T>
struct cast_overflow_lowest : ::testing::Test
{
    using First = typename T::first_type;
    using Second = typename T::second_type;
};
using OverflowLowestTypes = ::testing::Types<
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
TYPED_TEST_SUITE(cast_overflow_lowest, OverflowLowestTypes);

TYPED_TEST(cast_overflow_lowest, checked_exact_num_cast)
{
    using From = typename TestFixture::First;
    using To = typename TestFixture::Second;
    static constexpr auto from_lowest = std::numeric_limits<From>::lowest();

    EXPECT_THROW(safe_num_cast<To>(from_lowest), std::overflow_error);
}

TEST(cast, precision)
{
    EXPECT_THROW(safe_num_cast<int>(1.1), std::runtime_error);
    EXPECT_THROW(safe_num_cast<float>(std::nextafter(double(1), 2)), std::runtime_error);
}
