// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <stdexcept>

#include <doctest/doctest.h>

#include "mamba/specs/version_spec.hpp"

using namespace mamba::specs;

TEST_SUITE("version_spec")
{
    TEST_CASE("VersionInterval")
    {
        const auto v1 = Version::parse("1.0");
        const auto v2 = Version::parse("2.0");
        const auto v3 = Version::parse("3.0");
        const auto v4 = Version::parse("4.0");
        const auto v5 = Version::parse("5.0");

        using Bound = typename VersionInterval::Bound;

        const auto empty = VersionInterval::make_empty();
        const auto free = VersionInterval::make_free();
        const auto sing2 = VersionInterval::make_singleton(v2);
        const auto lb2c = VersionInterval::make_lower_bounded(v2, Bound::Closed);
        const auto lb2o = VersionInterval::make_lower_bounded(v2, Bound::Open);
        const auto ub2c = VersionInterval::make_upper_bounded(v2, Bound::Closed);
        const auto ub2o = VersionInterval::make_upper_bounded(v2, Bound::Open);
        const auto c2c4 = VersionInterval::make_bounded(v2, Bound::Closed, v4, Bound::Closed);
        const auto c2o4 = VersionInterval::make_bounded(v2, Bound::Closed, v4, Bound::Open);
        const auto o2c4 = VersionInterval::make_bounded(v2, Bound::Open, v4, Bound::Closed);
        const auto o2o4 = VersionInterval::make_bounded(v2, Bound::Open, v4, Bound::Open);
        // Degenerate same version
        const auto c2c2 = VersionInterval::make_bounded(v2, Bound::Closed, v2, Bound::Closed);
        const auto c2o2 = VersionInterval::make_bounded(v2, Bound::Closed, v2, Bound::Open);
        const auto o2c2 = VersionInterval::make_bounded(v2, Bound::Open, v2, Bound::Closed);
        const auto o2o2 = VersionInterval::make_bounded(v2, Bound::Open, v2, Bound::Open);
        // Degenerate wrong order
        const auto c4c2 = VersionInterval::make_bounded(v4, Bound::Closed, v2, Bound::Closed);
        const auto c4o2 = VersionInterval::make_bounded(v4, Bound::Closed, v2, Bound::Open);
        const auto o4c2 = VersionInterval::make_bounded(v4, Bound::Open, v2, Bound::Closed);
        const auto o4o2 = VersionInterval::make_bounded(v4, Bound::Open, v2, Bound::Open);

        SUBCASE("{}")
        {
            CHECK(empty.is_empty());
            CHECK_FALSE(empty.is_singleton());
            CHECK(empty.is_lower_bounded());
            CHECK(empty.is_upper_bounded());
            CHECK(empty.is_bounded());
            CHECK(empty.is_lower_closed());
            CHECK(empty.is_upper_closed());
            CHECK(empty.is_closed());
            CHECK(empty.is_segment());
            CHECK_EQ(empty, empty);
            // Degenerate
            CHECK_EQ(c2o2, empty);
            CHECK_EQ(o2c2, empty);
            CHECK_EQ(o2o2, empty);
            CHECK_EQ(c4c2, empty);
            CHECK_EQ(c4o2, empty);
            CHECK_EQ(o4c2, empty);
            CHECK_EQ(o4o2, empty);

            CHECK_FALSE(empty.contains(v2));
            CHECK_FALSE(c2o2.contains(v4));
        }

        SUBCASE("{2.0}")
        {
            CHECK_FALSE(sing2.is_empty());
            CHECK(sing2.is_singleton());
            CHECK(sing2.is_lower_bounded());
            CHECK(sing2.is_upper_bounded());
            CHECK(sing2.is_bounded());
            CHECK(sing2.is_lower_closed());
            CHECK(sing2.is_upper_closed());
            CHECK(sing2.is_closed());
            CHECK(sing2.is_segment());
            CHECK_EQ(sing2, sing2);
            CHECK_EQ(c2c2, sing2);

            CHECK(sing2.contains(v2));
            CHECK_FALSE(sing2.contains(v4));
            CHECK(c2c2.contains(v2));
        }

        SUBCASE("]-inf, +inf[")
        {
            CHECK_FALSE(free.is_empty());
            CHECK_FALSE(free.is_singleton());
            CHECK_FALSE(free.is_lower_bounded());
            CHECK_FALSE(free.is_upper_bounded());
            CHECK_FALSE(free.is_bounded());
            CHECK_FALSE(free.is_lower_closed());
            CHECK_FALSE(free.is_upper_closed());
            CHECK_FALSE(free.is_closed());
            CHECK_FALSE(free.is_segment());
            CHECK_EQ(free, free);

            CHECK(free.contains(v2));
            CHECK(free.contains(v4));
        }

        SUBCASE("]-inf, 2.0[")
        {
            CHECK_FALSE(ub2o.is_empty());
            CHECK_FALSE(ub2o.is_singleton());
            CHECK_FALSE(ub2o.is_lower_bounded());
            CHECK(ub2o.is_upper_bounded());
            CHECK_FALSE(ub2o.is_bounded());
            CHECK_FALSE(ub2o.is_lower_closed());
            CHECK_FALSE(ub2o.is_upper_closed());
            CHECK_FALSE(ub2o.is_closed());
            CHECK_FALSE(ub2o.is_segment());
            CHECK_EQ(ub2o, ub2o);

            CHECK(ub2o.contains(v1));
            CHECK_FALSE(ub2o.contains(v2));
            CHECK_FALSE(ub2o.contains(v3));
            CHECK_FALSE(ub2o.contains(v4));
            CHECK_FALSE(ub2o.contains(v5));
        }

        SUBCASE("]-inf, 2.0]")
        {
            CHECK_FALSE(ub2c.is_empty());
            CHECK_FALSE(ub2c.is_singleton());
            CHECK_FALSE(ub2c.is_lower_bounded());
            CHECK(ub2c.is_upper_bounded());
            CHECK_FALSE(ub2c.is_bounded());
            CHECK_FALSE(ub2c.is_lower_closed());
            CHECK(ub2c.is_upper_closed());
            CHECK_FALSE(ub2c.is_closed());
            CHECK_FALSE(ub2c.is_segment());
            CHECK_EQ(ub2c, ub2c);

            CHECK(ub2c.contains(v1));
            CHECK(ub2c.contains(v2));
            CHECK_FALSE(ub2c.contains(v3));
            CHECK_FALSE(ub2c.contains(v4));
            CHECK_FALSE(ub2c.contains(v5));
        }

        SUBCASE("]2.0, +inf[")
        {
            CHECK_FALSE(lb2o.is_empty());
            CHECK_FALSE(lb2o.is_singleton());
            CHECK(lb2o.is_lower_bounded());
            CHECK_FALSE(lb2o.is_upper_bounded());
            CHECK_FALSE(lb2o.is_bounded());
            CHECK_FALSE(lb2o.is_lower_closed());
            CHECK_FALSE(lb2o.is_upper_closed());
            CHECK_FALSE(lb2o.is_closed());
            CHECK_FALSE(lb2o.is_segment());
            CHECK_EQ(lb2o, lb2o);

            CHECK_FALSE(lb2o.contains(v1));
            CHECK_FALSE(lb2o.contains(v2));
            CHECK(lb2o.contains(v3));
            CHECK(lb2o.contains(v4));
            CHECK(lb2o.contains(v5));
        }

        SUBCASE("[2.0, +inf[")
        {
            CHECK_FALSE(lb2c.is_empty());
            CHECK_FALSE(lb2c.is_singleton());
            CHECK(lb2c.is_lower_bounded());
            CHECK_FALSE(lb2c.is_upper_bounded());
            CHECK_FALSE(lb2c.is_bounded());
            CHECK(lb2c.is_lower_closed());
            CHECK_FALSE(lb2c.is_upper_closed());
            CHECK_FALSE(lb2c.is_closed());
            CHECK_FALSE(lb2c.is_segment());
            CHECK_EQ(lb2c, lb2c);

            CHECK_FALSE(lb2c.contains(v1));
            CHECK(lb2c.contains(v2));
            CHECK(lb2c.contains(v3));
            CHECK(lb2c.contains(v4));
            CHECK(lb2c.contains(v5));
        }

        SUBCASE("[2.0, 4.0]")
        {
            CHECK_FALSE(c2c4.is_empty());
            CHECK_FALSE(c2c4.is_singleton());
            CHECK(c2c4.is_lower_bounded());
            CHECK(c2c4.is_upper_bounded());
            CHECK(c2c4.is_bounded());
            CHECK(c2c4.is_lower_closed());
            CHECK(c2c4.is_upper_closed());
            CHECK(c2c4.is_closed());
            CHECK(c2c4.is_segment());
            CHECK_EQ(c2c4, c2c4);

            CHECK_FALSE(c2c4.contains(v1));
            CHECK(c2c4.contains(v2));
            CHECK(c2c4.contains(v3));
            CHECK(c2c4.contains(v4));
            CHECK_FALSE(c2c4.contains(v5));
        }

        SUBCASE("[2.0, 4.0[")
        {
            CHECK_FALSE(c2o4.is_empty());
            CHECK_FALSE(c2o4.is_singleton());
            CHECK(c2o4.is_lower_bounded());
            CHECK(c2o4.is_upper_bounded());
            CHECK(c2o4.is_bounded());
            CHECK(c2o4.is_lower_closed());
            CHECK_FALSE(c2o4.is_upper_closed());
            CHECK_FALSE(c2o4.is_closed());
            CHECK_FALSE(c2o4.is_segment());
            CHECK_EQ(c2o4, c2o4);

            CHECK_FALSE(c2o4.contains(v1));
            CHECK(c2o4.contains(v2));
            CHECK(c2o4.contains(v3));
            CHECK_FALSE(c2o4.contains(v4));
            CHECK_FALSE(c2o4.contains(v5));
        }

        SUBCASE("]2.0, 4.0]")
        {
            CHECK_FALSE(o2c4.is_empty());
            CHECK_FALSE(o2c4.is_singleton());
            CHECK(o2c4.is_lower_bounded());
            CHECK(o2c4.is_upper_bounded());
            CHECK(o2c4.is_bounded());
            CHECK_FALSE(o2c4.is_lower_closed());
            CHECK(o2c4.is_upper_closed());
            CHECK_FALSE(o2c4.is_closed());
            CHECK_FALSE(o2c4.is_segment());
            CHECK_EQ(o2c4, o2c4);

            CHECK_FALSE(o2c4.contains(v1));
            CHECK_FALSE(o2c4.contains(v2));
            CHECK(o2c4.contains(v3));
            CHECK(o2c4.contains(v4));
            CHECK_FALSE(o2c4.contains(v5));
        }

        SUBCASE("]2.0, 4.0[")
        {
            CHECK_FALSE(o2o4.is_empty());
            CHECK_FALSE(o2o4.is_singleton());
            CHECK(o2o4.is_lower_bounded());
            CHECK(o2o4.is_upper_bounded());
            CHECK(o2o4.is_bounded());
            CHECK_FALSE(o2o4.is_lower_closed());
            CHECK_FALSE(o2o4.is_upper_closed());
            CHECK_FALSE(o2o4.is_closed());
            CHECK_FALSE(o2o4.is_segment());
            CHECK_EQ(o2o4, o2o4);

            CHECK_FALSE(o2o4.contains(v1));
            CHECK_FALSE(o2o4.contains(v2));
            CHECK(o2o4.contains(v3));
            CHECK_FALSE(o2o4.contains(v4));
            CHECK_FALSE(o2o4.contains(v5));
        }
    }
}
