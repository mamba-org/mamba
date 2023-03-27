// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <limits>
#include <utility>

#include <gtest/gtest.h>

#include "mamba/util/compare.hpp"

using namespace mamba::util;


TEST(compare, equal)
{
    EXPECT_TRUE(cmp_equal(char{ 0 }, char{ 0 }));
    EXPECT_TRUE(cmp_equal(char{ 1 }, char{ 1 }));
    EXPECT_TRUE(cmp_equal(char{ -1 }, char{ -1 }));
    EXPECT_TRUE(cmp_equal(int{ 0 }, int{ 0 }));
    EXPECT_TRUE(cmp_equal(int{ 1 }, int{ 1 }));
    EXPECT_TRUE(cmp_equal(int{ -1 }, int{ -1 }));
    EXPECT_TRUE(cmp_equal(std::size_t{ 0 }, std::size_t{ 0 }));
    EXPECT_TRUE(cmp_equal(std::size_t{ 1 }, std::size_t{ 1 }));

    EXPECT_TRUE(cmp_equal(char{ 0 }, int{ 0 }));
    EXPECT_TRUE(cmp_equal(char{ 1 }, int{ 1 }));
    EXPECT_TRUE(cmp_equal(char{ -1 }, int{ -1 }));
    EXPECT_TRUE(cmp_equal(std::size_t{ 0 }, char{ 0 }));
    EXPECT_TRUE(cmp_equal(std::size_t{ 1 }, char{ 1 }));
    EXPECT_TRUE(cmp_equal(std::size_t{ 0 }, int{ 0 }));
    EXPECT_TRUE(cmp_equal(std::size_t{ 1 }, int{ 1 }));

    EXPECT_FALSE(cmp_equal(char{ 0 }, char{ 1 }));
    EXPECT_FALSE(cmp_equal(char{ 1 }, char{ -1 }));
    EXPECT_FALSE(cmp_equal(int{ 0 }, int{ 1 }));
    EXPECT_FALSE(cmp_equal(int{ -1 }, int{ 1 }));
    EXPECT_FALSE(cmp_equal(std::size_t{ 0 }, std::size_t{ 1 }));

    EXPECT_FALSE(cmp_equal(char{ 0 }, int{ 1 }));
    EXPECT_FALSE(cmp_equal(char{ 1 }, int{ -1 }));
    EXPECT_FALSE(cmp_equal(char{ -1 }, int{ 1 }));
    EXPECT_FALSE(cmp_equal(std::size_t{ 1 }, int{ -1 }));
    EXPECT_FALSE(cmp_equal(static_cast<std::size_t>(-1), int{ -1 }));
    EXPECT_FALSE(cmp_equal(std::size_t{ 1 }, int{ 0 }));
    EXPECT_FALSE(cmp_equal(std::numeric_limits<std::size_t>::max(), int{ 0 }));
}

TEST(compare, less)
{
    EXPECT_TRUE(cmp_less(char{ 0 }, char{ 1 }));
    EXPECT_TRUE(cmp_less(char{ -1 }, char{ 0 }));
    EXPECT_TRUE(cmp_less(int{ 0 }, int{ 1 }));
    EXPECT_TRUE(cmp_less(int{ -1 }, int{ 1 }));
    EXPECT_TRUE(cmp_less(std::size_t{ 0 }, std::size_t{ 1 }));

    EXPECT_TRUE(cmp_less(char{ 0 }, int{ 1 }));
    EXPECT_TRUE(cmp_less(char{ -1 }, int{ 0 }));
    EXPECT_TRUE(cmp_less(char{ -1 }, int{ 1 }));
    EXPECT_TRUE(cmp_less(char{ -1 }, std::size_t{ 1 }));
    EXPECT_TRUE(cmp_less(std::size_t{ 0 }, int{ 1 }));
    EXPECT_TRUE(cmp_less(std::numeric_limits<int>::min(), char{ 0 }));
    EXPECT_TRUE(cmp_less(std::numeric_limits<int>::min(), std::size_t{ 0 }));
    EXPECT_TRUE(cmp_less(int{ -1 }, std::numeric_limits<std::size_t>::max()));
    EXPECT_TRUE(cmp_less(std::size_t{ 1 }, std::numeric_limits<int>::max()));

    EXPECT_FALSE(cmp_less(char{ 1 }, char{ 0 }));
    EXPECT_FALSE(cmp_less(char{ 1 }, char{ 1 }));
    EXPECT_FALSE(cmp_less(char{ 0 }, char{ -1 }));
    EXPECT_FALSE(cmp_less(int{ 1 }, int{ 0 }));
    EXPECT_FALSE(cmp_less(int{ 1 }, int{ -1 }));
    EXPECT_FALSE(cmp_less(std::size_t{ 1 }, std::size_t{ 0 }));

    EXPECT_FALSE(cmp_less(char{ 1 }, int{ 1 }));
    EXPECT_FALSE(cmp_less(char{ 1 }, int{ 0 }));
    EXPECT_FALSE(cmp_less(char{ 0 }, int{ -1 }));
    EXPECT_FALSE(cmp_less(char{ 1 }, int{ -11 }));
    EXPECT_FALSE(cmp_less(std::size_t{ 1 }, char{ -1 }));
    EXPECT_FALSE(cmp_less(int{ 1 }, std::size_t{ 0 }));
    EXPECT_FALSE(cmp_less(char{ 0 }, std::numeric_limits<int>::min()));
    EXPECT_FALSE(cmp_less(std::size_t{ 0 }, std::numeric_limits<int>::min()));
    EXPECT_FALSE(cmp_less(std::numeric_limits<std::size_t>::max(), int{ -1 }));
    EXPECT_FALSE(cmp_less(std::numeric_limits<int>::max(), std::size_t{ 1 }));
}
