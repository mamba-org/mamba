// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <type_traits>

#include <gtest/gtest.h>

#include "mamba/util/flat_set.hpp"

using namespace mamba::util;

TEST(flat_set, constructor)
{
    const auto s1 = flat_set<int>();
    EXPECT_EQ(s1.size(), 0);
    auto s2 = flat_set<int>({ 1, 2 });
    EXPECT_EQ(s2.size(), 2);
    const auto s3 = flat_set<int>{ s2 };
    EXPECT_EQ(s3.size(), 2);
    const auto s4 = flat_set<int>{ std::move(s2) };
    EXPECT_EQ(s4.size(), 2);
    // CTAD
    auto s5 = flat_set({ 1, 2 });
    EXPECT_EQ(s5.size(), 2);
    static_assert(std::is_same_v<decltype(s5)::value_type, int>);
    auto s6 = flat_set(s5.begin(), s5.end(), std::greater{});
    EXPECT_EQ(s6.size(), s5.size());
    static_assert(std::is_same_v<decltype(s6)::value_type, decltype(s5)::value_type>);
}

TEST(flat_set, equality)
{
    EXPECT_EQ(flat_set<int>(), flat_set<int>());
    EXPECT_EQ(flat_set<int>({ 1, 2 }), flat_set<int>({ 1, 2 }));
    EXPECT_EQ(flat_set<int>({ 1, 2 }), flat_set<int>({ 2, 1 }));
    EXPECT_EQ(flat_set<int>({ 1, 2, 1 }), flat_set<int>({ 2, 2, 1 }));
    EXPECT_NE(flat_set<int>({ 1, 2 }), flat_set<int>({ 1, 2, 3 }));
    EXPECT_NE(flat_set<int>({ 2 }), flat_set<int>({}));
}

TEST(flat_set, insert)
{
    auto s = flat_set<int>();
    s.insert(33);
    EXPECT_EQ(s, flat_set<int>({ 33 }));
    s.insert(33);
    s.insert(17);
    EXPECT_EQ(s, flat_set<int>({ 17, 33 }));
    s.insert(22);
    EXPECT_EQ(s, flat_set<int>({ 17, 22, 33 }));
    s.insert(33);
    EXPECT_EQ(s, flat_set<int>({ 17, 22, 33 }));
    auto v = std::vector<int>({ 33, 22, 17, 0 });
    s.insert(v.begin(), v.end());
    EXPECT_EQ(s, flat_set<int>({ 0, 17, 22, 33 }));
}

TEST(flat_set, erase)
{
    auto s = flat_set<int>{ 4, 3, 2, 1 };
    EXPECT_EQ(s.erase(4), 1);
    EXPECT_EQ(s, flat_set<int>({ 1, 2, 3 }));
    EXPECT_EQ(s.erase(4), 0);
    EXPECT_EQ(s, flat_set<int>({ 1, 2, 3 }));

    const auto it = s.erase(s.begin());
    EXPECT_EQ(it, s.begin());
    EXPECT_EQ(s, flat_set<int>({ 2, 3 }));
}

TEST(flat_set, contains)
{
    const auto s = flat_set<int>({ 1, 3, 4, 5 });
    EXPECT_FALSE(s.contains(0));
    EXPECT_TRUE(s.contains(1));
    EXPECT_FALSE(s.contains(2));
    EXPECT_TRUE(s.contains(3));
    EXPECT_TRUE(s.contains(4));
    EXPECT_TRUE(s.contains(5));
    EXPECT_FALSE(s.contains(6));
}

TEST(flat_set, key_compare)
{
    auto s = flat_set({ 1, 3, 4, 5 }, std::greater{});
    EXPECT_EQ(s.front(), 5);
    EXPECT_EQ(s.back(), 1);
    s.insert(6);
    EXPECT_EQ(s.front(), 6);
}
