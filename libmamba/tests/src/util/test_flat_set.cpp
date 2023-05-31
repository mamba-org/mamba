// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <type_traits>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/util/flat_set.hpp"

#include "doctest-printer/flat_set.hpp"

using namespace mamba::util;

TEST_SUITE("util::flat_set")
{
    TEST_CASE("constructor")
    {
        const auto s1 = flat_set<int>();
        CHECK_EQ(s1.size(), 0);
        auto s2 = flat_set<int>({ 1, 2 });
        CHECK_EQ(s2.size(), 2);
        const auto s3 = flat_set<int>{ s2 };
        CHECK_EQ(s3.size(), 2);
        const auto s4 = flat_set<int>{ std::move(s2) };
        CHECK_EQ(s4.size(), 2);
        // CTAD
        auto s5 = flat_set({ 1, 2 });
        CHECK_EQ(s5.size(), 2);
        static_assert(std::is_same_v<decltype(s5)::value_type, int>);
        auto s6 = flat_set(s5.begin(), s5.end(), std::greater{});
        CHECK_EQ(s6.size(), s5.size());
        static_assert(std::is_same_v<decltype(s6)::value_type, decltype(s5)::value_type>);
    }

    TEST_CASE("equality")
    {
        CHECK_EQ(flat_set<int>(), flat_set<int>());
        CHECK_EQ(flat_set<int>({ 1, 2 }), flat_set<int>({ 1, 2 }));
        CHECK_EQ(flat_set<int>({ 1, 2 }), flat_set<int>({ 2, 1 }));
        CHECK_EQ(flat_set<int>({ 1, 2, 1 }), flat_set<int>({ 2, 2, 1 }));
        CHECK_NE(flat_set<int>({ 1, 2 }), flat_set<int>({ 1, 2, 3 }));
        CHECK_NE(flat_set<int>({ 2 }), flat_set<int>({}));
    }

    TEST_CASE("insert")
    {
        auto s = flat_set<int>();
        s.insert(33);
        CHECK_EQ(s, flat_set<int>({ 33 }));
        s.insert(33);
        s.insert(17);
        CHECK_EQ(s, flat_set<int>({ 17, 33 }));
        s.insert(22);
        CHECK_EQ(s, flat_set<int>({ 17, 22, 33 }));
        s.insert(33);
        CHECK_EQ(s, flat_set<int>({ 17, 22, 33 }));
        auto v = std::vector<int>({ 33, 22, 17, 0 });
        s.insert(v.begin(), v.end());
        CHECK_EQ(s, flat_set<int>({ 0, 17, 22, 33 }));
    }

    TEST_CASE("insert conversion")
    {
        auto s = flat_set<std::string>();
        const auto v = std::array<std::string_view, 2>{ "hello", "world" };
        s.insert(v.cbegin(), v.cend());
        REQUIRE_EQ(s.size(), 2);
        CHECK_EQ(s.at(0), "hello");
        CHECK_EQ(s.at(1), "world");
    }

    TEST_CASE("erase")
    {
        auto s = flat_set<int>{ 4, 3, 2, 1 };
        CHECK_EQ(s.erase(4), 1);
        CHECK_EQ(s, flat_set<int>({ 1, 2, 3 }));
        CHECK_EQ(s.erase(4), 0);
        CHECK_EQ(s, flat_set<int>({ 1, 2, 3 }));

        const auto it = s.erase(s.begin());
        CHECK_EQ(it, s.begin());
        CHECK_EQ(s, flat_set<int>({ 2, 3 }));
    }

    TEST_CASE("contains")
    {
        const auto s = flat_set<int>({ 1, 3, 4, 5 });
        CHECK_FALSE(s.contains(0));
        CHECK(s.contains(1));
        CHECK_FALSE(s.contains(2));
        CHECK(s.contains(3));
        CHECK(s.contains(4));
        CHECK(s.contains(5));
        CHECK_FALSE(s.contains(6));
    }

    TEST_CASE("key_compare")
    {
        auto s = flat_set({ 1, 3, 4, 5 }, std::greater{});
        CHECK_EQ(s.front(), 5);
        CHECK_EQ(s.back(), 1);
        s.insert(6);
        CHECK_EQ(s.front(), 6);
    }

    TEST_CASE("Set operations")
    {
        const auto s1 = flat_set<int>({ 1, 3, 4, 5 });
        const auto s2 = flat_set<int>({ 3, 5 });
        const auto s3 = flat_set<int>({ 4, 6 });

        SUBCASE("Disjoint")
        {
            CHECK(s1.is_disjoint_of(flat_set<int>{}));
            CHECK_FALSE(s1.is_disjoint_of(s1));
            CHECK_FALSE(s1.is_disjoint_of(s2));
            CHECK_FALSE(s1.is_disjoint_of(s3));
            CHECK(s2.is_disjoint_of(s3));
            CHECK(s3.is_disjoint_of(s2));
        }

        SUBCASE("Subset")
        {
            CHECK_LE(s1, s1);
            CHECK_FALSE(s1 < s1);
            CHECK_LE(flat_set<int>{}, s1);
            CHECK_LT(flat_set<int>{}, s1);
            CHECK_FALSE(s1 <= s2);
            CHECK_FALSE(s1 <= flat_set<int>{});
            CHECK_LE(flat_set<int>{ 1, 4 }, s1);
            CHECK_LT(flat_set<int>{ 1, 4 }, s1);
            CHECK_LE(s2, s1);
            CHECK_LT(s2, s1);
        }

        SUBCASE("Superset")
        {
            CHECK_GE(s1, s1);
            CHECK_FALSE(s1 > s1);
            CHECK_GE(s1, flat_set<int>{});
            CHECK_GT(s1, flat_set<int>{});
            CHECK_FALSE(s2 >= s1);
            CHECK_FALSE(flat_set<int>{} >= s1);
            CHECK_GE(s1, flat_set<int>{ 1, 4 });
            CHECK_GT(s1, flat_set<int>{ 1, 4 });
            CHECK_GE(s1, s2);
            CHECK_GT(s1, s2);
        }

        SUBCASE("Union")
        {
            CHECK_EQ(s1 | s1, s1);
            CHECK_EQ(s1 | s2, s1);
            CHECK_EQ(s2 | s1, s1 | s2);
            CHECK_EQ(s1 | s3, flat_set<int>{ 1, 3, 4, 5, 6 });
            CHECK_EQ(s3 | s1, s1 | s3);
            CHECK_EQ(s2 | s3, flat_set<int>{ 3, 4, 5, 6 });
            CHECK_EQ(s3 | s2, s2 | s3);
        }

        SUBCASE("Intersection")
        {
            CHECK_EQ(s1 & s1, s1);
            CHECK_EQ(s1 & s2, s2);
            CHECK_EQ(s2 & s1, s1 & s2);
            CHECK_EQ(s1 & s3, flat_set<int>{ 4 });
            CHECK_EQ(s3 & s1, s1 & s3);
            CHECK_EQ(s2 & s3, flat_set<int>{});
            CHECK_EQ(s3 & s2, s2 & s3);
        }

        SUBCASE("Difference")
        {
            CHECK_EQ(s1 - s1, flat_set<int>{});
            CHECK_EQ(s1 - s2, flat_set<int>{ 1, 4 });
            CHECK_EQ(s2 - s1, flat_set<int>{});
            CHECK_EQ(s1 - s3, flat_set<int>{ 1, 3, 5 });
            CHECK_EQ(s3 - s1, flat_set<int>{ 6 });
            CHECK_EQ(s2 - s3, s2);
            CHECK_EQ(s3 - s2, s3);
        }

        SUBCASE("Symetric difference")
        {
            CHECK_EQ(s1 ^ s1, flat_set<int>{});
            CHECK_EQ(s1 ^ s2, flat_set<int>{ 1, 4 });
            CHECK_EQ(s2 ^ s1, s1 ^ s2);
            CHECK_EQ(s1 ^ s3, flat_set<int>{ 1, 3, 5, 6 });
            CHECK_EQ(s3 ^ s1, s1 ^ s3);
            CHECK_EQ(s2 ^ s3, flat_set<int>{ 3, 4, 5, 6 });
            CHECK_EQ(s3 ^ s2, s2 ^ s3);
        }

        SUBCASE("Algebra")
        {
            for (const auto& u : { s1, s2, s3 })
            {
                for (const auto& v : { s1, s2, s3 })
                {
                    CHECK_EQ((u - v) | (v - u) | (u & v), u | v);
                    CHECK_EQ((u ^ v) | (u & v), u | v);
                    CHECK_EQ((u | v) - (u & v), u ^ v);
                }
            }
        }
    }
}
