// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <list>
#include <stdexcept>
#include <vector>

#include <doctest/doctest.h>

#include "solv-cpp/queue.hpp"

using namespace solv;

TEST_SUITE("solv::ObjQueue")
{
    TEST_CASE("constructor")
    {
        auto q1 = ObjQueue();
        CHECK_EQ(q1.size(), 0);
        CHECK(q1.empty());

        auto q2 = ObjQueue{ 1, 2, 3 };
        CHECK_EQ(q2.size(), 3);
        CHECK_FALSE(q2.empty());

        auto q3 = q2;
        CHECK_EQ(q3.size(), q2.size());
        CHECK_NE(q2.data(), q3.data());

        const auto q3_data = q3.data();
        const auto q3_size = q3.size();
        auto q4 = std::move(q3);
        CHECK_EQ(q4.size(), q3_size);
        CHECK_EQ(q4.data(), q3_data);

        const auto q4_data = q4.data();
        const auto q4_size = q4.size();
        q1 = std::move(q4);
        CHECK_EQ(q1.size(), q4_size);
        CHECK_EQ(q1.data(), q4_data);
    }

    TEST_CASE("swap")
    {
        auto q1 = ObjQueue();
        const auto q1_data = q1.data();
        const auto q1_size = q1.size();

        auto q2 = ObjQueue{ 1, 2, 3 };
        const auto q2_size = q2.size();
        const auto q2_data = q2.data();

        swap(q1, q2);
        CHECK_EQ(q1.size(), q2_size);
        CHECK_EQ(q1.data(), q2_data);
        CHECK_EQ(q2.size(), q1_size);
        CHECK_EQ(q2.data(), q1_data);
    }

    TEST_CASE("push_back")
    {
        auto q = ObjQueue();
        q.push_back(1);
        CHECK_EQ(q.front(), 1);
        CHECK_EQ(q.back(), 1);
        q.push_back(3);
        CHECK_EQ(q.front(), 1);
        CHECK_EQ(q.back(), 3);
    }

    TEST_CASE("element")
    {
        auto q = ObjQueue{ 3, 2, 1 };
        CHECK_EQ(q[0], 3);
        CHECK_EQ(q[1], 2);
        CHECK_EQ(q[2], 1);
    }

    TEST_CASE("at")
    {
        auto q = ObjQueue{ 3, 2, 1 };
        CHECK_EQ(q.at(0), q[0]);
        CHECK_EQ(q.at(1), q[1]);
        CHECK_EQ(q.at(2), q[2]);
        auto use_at = [&]() { [[maybe_unused]] const auto& x = q.at(q.size()); };
        CHECK_THROWS_AS(use_at(), std::out_of_range);
    }

    TEST_CASE("clear")
    {
        auto q = ObjQueue{ 3, 2, 1 };
        q.clear();
        CHECK(q.empty());
    }

    TEST_CASE("iterator")
    {
        const auto q = ObjQueue{ 3, 2, 1 };
        std::size_t n = 0;
        for ([[maybe_unused]] auto _ : q)
        {
            ++n;
        }
        CHECK_EQ(n, q.size());

        const auto l = std::list<::Id>(q.begin(), q.end());
        const auto l_expected = std::list{ 3, 2, 1 };
        CHECK_EQ(l, l_expected);
    }

    TEST_CASE("reverse_iterator")
    {
        const auto q = ObjQueue{ 3, 2, 1 };

        const auto v = std::vector(q.crbegin(), q.crend());
        CHECK_EQ(v.front(), q.back());
        CHECK_EQ(v.back(), q.front());
    }

    TEST_CASE("insert_one")
    {
        auto q = ObjQueue();
        auto iter = q.insert(q.cbegin(), 4);
        CHECK_EQ(*iter, 4);
        CHECK_EQ(q.front(), 4);
    }

    TEST_CASE("insert_span")
    {
        auto q = ObjQueue();

        const auto r1 = std::vector{ 1, 2, 3 };
        // std::vector::iterator is not always a pointer
        auto iter = q.insert(q.cend(), r1.data(), r1.data() + r1.size());
        CHECK_EQ(*iter, q[0]);
        CHECK_EQ(q[0], 1);
        CHECK_EQ(q[1], 2);
        CHECK_EQ(q[2], 3);

        const auto r2 = std::vector{ 4, 4 };
        iter = q.insert(q.cbegin(), r2.data(), r2.data() + r2.size());
        CHECK_EQ(*iter, q[0]);
        CHECK_EQ(q[0], 4);
        CHECK_EQ(q[1], 4);

        const auto r3 = std::vector<int>{};
        iter = q.insert(q.cbegin(), r3.data(), r3.data() + r3.size());
        CHECK_EQ(*iter, q[0]);
        CHECK_EQ(q[0], 4);
    }

    TEST_CASE("insert_range")
    {
        auto q = ObjQueue();

        const auto r1 = std::list{ 1, 2, 3 };
        auto iter = q.insert(q.cend(), r1.begin(), r1.end());
        CHECK_EQ(*iter, q[0]);
        CHECK_EQ(q[0], 1);
        CHECK_EQ(q[1], 2);
        CHECK_EQ(q[2], 3);

        const auto r2 = std::list{ 4, 4 };
        iter = q.insert(q.cbegin(), r2.begin(), r2.end());
        CHECK_EQ(*iter, q[0]);
        CHECK_EQ(q[0], 4);
        CHECK_EQ(q[1], 4);

        const auto r3 = std::list<int>{};
        iter = q.insert(q.cbegin(), r3.begin(), r3.end());
        CHECK_EQ(*iter, q[0]);
        CHECK_EQ(q[0], 4);
    }

    TEST_CASE("erase")
    {
        auto q = ObjQueue{ 3, 2, 1 };
        const auto iter = q.erase(q.cbegin() + 1);
        CHECK_EQ(*iter, 1);
        CHECK_EQ(q.size(), 2);
    }

    TEST_CASE("capacity")
    {
        auto q = ObjQueue();
        q.reserve(10);
        CHECK_EQ(q.size(), 0);
        CHECK_GE(q.capacity(), 10);
    }

    TEST_CASE("comparison")
    {
        CHECK_EQ(ObjQueue{}, ObjQueue{});

        auto q1 = ObjQueue{ 1, 2, 3 };

        CHECK_EQ(q1, q1);
        CHECK_NE(q1, ObjQueue{});

        auto q2 = q1;
        CHECK_EQ(q1, q2);
        q2.reserve(10);
        CHECK_EQ(q1, q2);
    }

    TEST_CASE("contains")
    {
        const auto q = ObjQueue{ 1, 9, 3 };
        CHECK(q.contains(3));
        CHECK_FALSE(q.contains(0));
    }
}
