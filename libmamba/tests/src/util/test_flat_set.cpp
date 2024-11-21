// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <type_traits>
#include <vector>

#include <catch2/catch_all.hpp>

#include "mamba/util/flat_set.hpp"

using namespace mamba::util;

namespace
{
    TEST_CASE("constructor")
    {
        const auto s1 = flat_set<int>();
        REQUIRE(s1.size() == 0);
        auto s2 = flat_set<int>({ 1, 2 });
        REQUIRE(s2.size() == 2);
        const auto s3 = flat_set<int>{ s2 };
        REQUIRE(s3.size() == 2);
        const auto s4 = flat_set<int>{ std::move(s2) };
        REQUIRE(s4.size() == 2);
        // CTAD
        auto s5 = flat_set({ 1, 2 });
        REQUIRE(s5.size() == 2);
        static_assert(std::is_same_v<decltype(s5)::value_type, int>);
        auto s6 = flat_set(s5.begin(), s5.end(), std::greater{});
        REQUIRE(s6.size() == s5.size());
        static_assert(std::is_same_v<decltype(s6)::value_type, decltype(s5)::value_type>);
    }

    TEST_CASE("equality")
    {
        REQUIRE(flat_set<int>() == flat_set<int>());
        REQUIRE(flat_set<int>({ 1, 2 }) == flat_set<int>({ 1, 2 }));
        REQUIRE(flat_set<int>({ 1, 2 }) == flat_set<int>({ 2, 1 }));
        REQUIRE(flat_set<int>({ 1, 2, 1 }) == flat_set<int>({ 2, 2, 1 }));
        REQUIRE(flat_set<int>({ 1, 2 }) != flat_set<int>({ 1, 2, 3 }));
        REQUIRE(flat_set<int>({ 2 }) != flat_set<int>({}));
    }

    TEST_CASE("insert")
    {
        auto s = flat_set<int>();
        s.insert(33);
        REQUIRE(s == flat_set<int>({ 33 }));
        s.insert(33);
        s.insert(17);
        REQUIRE(s == flat_set<int>({ 17, 33 }));
        s.insert(22);
        REQUIRE(s == flat_set<int>({ 17, 22, 33 }));
        s.insert(33);
        REQUIRE(s == flat_set<int>({ 17, 22, 33 }));
        auto v = std::vector<int>({ 33, 22, 17, 0 });
        s.insert(v.begin(), v.end());
        REQUIRE(s == flat_set<int>({ 0, 17, 22, 33 }));
    }

    TEST_CASE("insert conversion")
    {
        auto s = flat_set<std::string>();
        const auto v = std::array<std::string_view, 2>{ "hello", "world" };
        s.insert(v.cbegin(), v.cend());
        REQUIRE(s.size() == 2);
        REQUIRE(s.at(0) == "hello");
        REQUIRE(s.at(1) == "world");
    }

    TEST_CASE("erase")
    {
        auto s = flat_set<int>{ 4, 3, 2, 1 };
        REQUIRE(s.erase(4) == 1);
        REQUIRE(s == flat_set<int>({ 1, 2, 3 }));
        REQUIRE(s.erase(4) == 0);
        REQUIRE(s == flat_set<int>({ 1, 2, 3 }));

        const auto it = s.erase(s.begin());
        REQUIRE(it == s.begin());
        REQUIRE(s == flat_set<int>({ 2, 3 }));
    }

    TEST_CASE("set_contains")
    {
        const auto s = flat_set<int>({ 1, 3, 4, 5 });
        REQUIRE_FALSE(s.contains(0));
        REQUIRE(s.contains(1));
        REQUIRE_FALSE(s.contains(2));
        REQUIRE(s.contains(3));
        REQUIRE(s.contains(4));
        REQUIRE(s.contains(5));
        REQUIRE_FALSE(s.contains(6));
    }

    TEST_CASE("key_compare")
    {
        auto s = flat_set({ 1, 3, 4, 5 }, std::greater{});
        REQUIRE(s.front() == 5);
        REQUIRE(s.back() == 1);
        s.insert(6);
        REQUIRE(s.front() == 6);
    }

    TEST_CASE("Set operations")
    {
        const auto s1 = flat_set<int>({ 1, 3, 4, 5 });
        const auto s2 = flat_set<int>({ 3, 5 });
        const auto s3 = flat_set<int>({ 4, 6 });

        SECTION("Disjoint")
        {
            REQUIRE(set_is_disjoint_of(s1, flat_set<int>{}));
            REQUIRE_FALSE(set_is_disjoint_of(s1, s1));
            REQUIRE_FALSE(set_is_disjoint_of(s1, s2));
            REQUIRE_FALSE(set_is_disjoint_of(s1, s3));
            REQUIRE(set_is_disjoint_of(s2, s3));
            REQUIRE(set_is_disjoint_of(s3, s2));
        }

        SECTION("Subset")
        {
            REQUIRE(set_is_subset_of(s1, s1));
            REQUIRE_FALSE(set_is_strict_subset_of(s1, s1));
            REQUIRE(set_is_subset_of(flat_set<int>{}, s1));
            REQUIRE(set_is_strict_subset_of(flat_set<int>{}, s1));
            REQUIRE_FALSE(set_is_subset_of(s1, s2));
            REQUIRE_FALSE(set_is_subset_of(s1, flat_set<int>{}));
            REQUIRE(set_is_subset_of(flat_set<int>{ 1, 4 }, s1));
            REQUIRE(set_is_strict_subset_of(flat_set<int>{ 1, 4 }, s1));
            REQUIRE(set_is_subset_of(s2, s1));
            REQUIRE(set_is_strict_subset_of(s2, s1));
        }

        SECTION("Superset")
        {
            REQUIRE(set_is_superset_of(s1, s1));
            REQUIRE_FALSE(set_is_strict_superset_of(s1, s1));
            REQUIRE(set_is_superset_of(s1, flat_set<int>{}));
            REQUIRE(set_is_strict_superset_of(s1, flat_set<int>{}));
            REQUIRE_FALSE(set_is_superset_of(s2, s1));
            REQUIRE_FALSE(set_is_superset_of(flat_set<int>{}, s1));
            REQUIRE(set_is_superset_of(s1, flat_set<int>{ 1, 4 }));
            REQUIRE(set_is_strict_superset_of(s1, flat_set<int>{ 1, 4 }));
            REQUIRE(set_is_superset_of(s1, s2));
            REQUIRE(set_is_strict_superset_of(s1, s2));
        }

        SECTION("Union")
        {
            REQUIRE(set_union(s1, s1) == s1);
            REQUIRE(set_union(s1, s2) == s1);
            REQUIRE(set_union(s2, s1) == set_union(s1, s2));
            REQUIRE(set_union(s1, s3) == flat_set<int>{ 1, 3, 4, 5, 6 });
            REQUIRE(set_union(s3, s1) == set_union(s1, s3));
            REQUIRE(set_union(s2, s3) == flat_set<int>{ 3, 4, 5, 6 });
            REQUIRE(set_union(s3, s2) == set_union(s2, s3));
        }

        SECTION("Intersection")
        {
            REQUIRE(set_intersection(s1, s1) == s1);
            REQUIRE(set_intersection(s1, s2) == s2);
            REQUIRE(set_intersection(s2, s1) == set_intersection(s1, s2));
            REQUIRE(set_intersection(s1, s3) == flat_set<int>{ 4 });
            REQUIRE(set_intersection(s3, s1) == set_intersection(s1, s3));
            REQUIRE(set_intersection(s2, s3) == flat_set<int>{});
            REQUIRE(set_intersection(s3, s2) == set_intersection(s2, s3));
        }

        SECTION("Difference")
        {
            REQUIRE(set_difference(s1, s1) == flat_set<int>{});
            REQUIRE(set_difference(s1, s2) == flat_set<int>{ 1, 4 });
            REQUIRE(set_difference(s2, s1) == flat_set<int>{});
            REQUIRE(set_difference(s1, s3) == flat_set<int>{ 1, 3, 5 });
            REQUIRE(set_difference(s3, s1) == flat_set<int>{ 6 });
            REQUIRE(set_difference(s2, s3) == s2);
            REQUIRE(set_difference(s3, s2) == s3);
        }

        SECTION("Symmetric difference")
        {
            REQUIRE(set_symmetric_difference(s1, s1) == flat_set<int>{});
            REQUIRE(set_symmetric_difference(s1, s2) == flat_set<int>{ 1, 4 });
            REQUIRE(set_symmetric_difference(s2, s1) == set_symmetric_difference(s1, s2));
            REQUIRE(set_symmetric_difference(s1, s3) == flat_set<int>{ 1, 3, 5, 6 });
            REQUIRE(set_symmetric_difference(s3, s1) == set_symmetric_difference(s1, s3));
            REQUIRE(set_symmetric_difference(s2, s3) == flat_set<int>{ 3, 4, 5, 6 });
            REQUIRE(set_symmetric_difference(s3, s2) == set_symmetric_difference(s2, s3));
        }

        SECTION("Algebra")
        {
            for (const auto& u : { s1, s2, s3 })
            {
                for (const auto& v : { s1, s2, s3 })
                {
                    REQUIRE(
                        set_union(
                            set_difference(u, v),
                            set_union(set_difference(v, u), set_intersection(u, v))
                        )
                        == set_union(u, v)
                    );
                    REQUIRE(
                        set_union(set_symmetric_difference(u, v), set_intersection(u, v))
                        == set_union(u, v)
                    );
                    REQUIRE(
                        set_difference(set_union(u, v), set_intersection(u, v))
                        == set_symmetric_difference(u, v)
                    );
                }
            }
        }
    }
}
