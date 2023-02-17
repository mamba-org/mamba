// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <list>
#include <vector>

#include <gtest/gtest.h>

#include "solv-cpp/queue.hpp"

using namespace mamba::solv;

TEST(ObjQueue, constructor)
{
    auto q1 = ObjQueue();
    EXPECT_EQ(q1.size(), 0);
    EXPECT_TRUE(q1.empty());

    auto q2 = ObjQueue{ 1, 2, 3 };
    EXPECT_EQ(q2.size(), 3);
    EXPECT_FALSE(q2.empty());

    auto q3 = q2;
    EXPECT_EQ(q3.size(), q2.size());
    EXPECT_NE(q2.data(), q3.data());

    const auto q3_data = q3.data();
    const auto q3_size = q3.size();
    auto q4 = std::move(q3);
    EXPECT_EQ(q4.size(), q3_size);
    EXPECT_EQ(q4.data(), q3_data);

    const auto q4_data = q4.data();
    const auto q4_size = q4.size();
    q1 = std::move(q4);
    EXPECT_EQ(q1.size(), q4_size);
    EXPECT_EQ(q1.data(), q4_data);
}

TEST(ObjQueue, swap)
{
    auto q1 = ObjQueue();
    const auto q1_data = q1.data();
    const auto q1_size = q1.size();

    auto q2 = ObjQueue{ 1, 2, 3 };
    const auto q2_size = q2.size();
    const auto q2_data = q2.data();

    swap(q1, q2);
    EXPECT_EQ(q1.size(), q2_size);
    EXPECT_EQ(q1.data(), q2_data);
    EXPECT_EQ(q2.size(), q1_size);
    EXPECT_EQ(q2.data(), q1_data);
}

TEST(ObjQueue, push_back)
{
    auto q = ObjQueue();
    q.push_back(1);
    EXPECT_EQ(q.front(), 1);
    EXPECT_EQ(q.back(), 1);
    q.push_back(3);
    EXPECT_EQ(q.front(), 1);
    EXPECT_EQ(q.back(), 3);
}

TEST(ObjQueue, element)
{
    auto q = ObjQueue{ 3, 2, 1 };
    EXPECT_EQ(q[0], 3);
    EXPECT_EQ(q[1], 2);
    EXPECT_EQ(q[2], 1);
}

TEST(ObjQueue, clear)
{
    auto q = ObjQueue{ 3, 2, 1 };
    q.clear();
    EXPECT_TRUE(q.empty());
}

TEST(ObjQueue, iterator)
{
    const auto q = ObjQueue{ 3, 2, 1 };
    std::size_t n = 0;
    for ([[maybe_unused]] auto _ : q)
    {
        ++n;
    }
    EXPECT_EQ(n, q.size());

    const auto l = std::list<::Id>(q.begin(), q.end());
    const auto l_expected = std::list{ 3, 2, 1 };
    EXPECT_EQ(l, l_expected);
}

TEST(ObjQueue, reverse_iterator)
{
    const auto q = ObjQueue{ 3, 2, 1 };

    const auto v = std::vector(q.crbegin(), q.crend());
    EXPECT_EQ(v.front(), q.back());
    EXPECT_EQ(v.back(), q.front());
}

TEST(ObjQueue, insert_one)
{
    auto q = ObjQueue();
    auto iter = q.insert(q.cbegin(), 4);
    EXPECT_EQ(*iter, 4);
    EXPECT_EQ(q.front(), 4);
}

TEST(ObjQueue, insert_span)
{
    auto q = ObjQueue();

    const auto r1 = std::vector{ 1, 2, 3 };
    // std::vector::iterator is not always a pointer
    auto iter = q.insert(q.cend(), r1.data(), r1.data() + r1.size());
    EXPECT_EQ(*iter, q[0]);
    EXPECT_EQ(q[0], 1);
    EXPECT_EQ(q[1], 2);
    EXPECT_EQ(q[2], 3);

    const auto r2 = std::vector{ 4, 4 };
    iter = q.insert(q.cbegin(), r2.data(), r2.data() + r2.size());
    EXPECT_EQ(*iter, q[0]);
    EXPECT_EQ(q[0], 4);
    EXPECT_EQ(q[1], 4);

    const auto r3 = std::vector<int>{};
    iter = q.insert(q.cbegin(), r3.data(), r3.data() + r3.size());
    EXPECT_EQ(*iter, q[0]);
    EXPECT_EQ(q[0], 4);
}

TEST(ObjQueue, insert_range)
{
    auto q = ObjQueue();

    const auto r1 = std::list{ 1, 2, 3 };
    auto iter = q.insert(q.cend(), r1.begin(), r1.end());
    EXPECT_EQ(*iter, q[0]);
    EXPECT_EQ(q[0], 1);
    EXPECT_EQ(q[1], 2);
    EXPECT_EQ(q[2], 3);

    const auto r2 = std::list{ 4, 4 };
    iter = q.insert(q.cbegin(), r2.begin(), r2.end());
    EXPECT_EQ(*iter, q[0]);
    EXPECT_EQ(q[0], 4);
    EXPECT_EQ(q[1], 4);

    const auto r3 = std::list<int>{};
    iter = q.insert(q.cbegin(), r3.begin(), r3.end());
    EXPECT_EQ(*iter, q[0]);
    EXPECT_EQ(q[0], 4);
}

TEST(ObjQueue, erase)
{
    auto q = ObjQueue{ 3, 2, 1 };
    const auto iter = q.erase(q.cbegin() + 1);
    EXPECT_EQ(*iter, 1);
    EXPECT_EQ(q.size(), 2);
}

TEST(ObjQueue, capacity)
{
    auto q = ObjQueue();
    q.reserve(10);
    EXPECT_EQ(q.size(), 0);
    EXPECT_GE(q.capacity(), 10);
}
