// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>

#include <catch2/catch_all.hpp>

#include "mamba/specs/build_number_spec.hpp"

using namespace mamba::specs;

namespace
{
    TEST_CASE("BuildNumberPredicate")
    {
        const auto free = BuildNumberPredicate::make_free();
        REQUIRE(free.contains(0));
        REQUIRE(free.contains(1));
        REQUIRE(free.contains(2));
        CHECK_EQ(free.str(), "=*");

        const auto eq = BuildNumberPredicate::make_equal_to(1);
        REQUIRE_FALSE(eq.contains(0));
        REQUIRE(eq.contains(1));
        REQUIRE_FALSE(eq.contains(2));
        CHECK_EQ(eq.str(), "=1");

        const auto ne = BuildNumberPredicate::make_not_equal_to(1);
        REQUIRE(ne.contains(0));
        REQUIRE_FALSE(ne.contains(1));
        REQUIRE(ne.contains(2));
        CHECK_EQ(ne.str(), "!=1");

        const auto gt = BuildNumberPredicate::make_greater(1);
        REQUIRE_FALSE(gt.contains(0));
        REQUIRE_FALSE(gt.contains(1));
        REQUIRE(gt.contains(2));
        CHECK_EQ(gt.str(), ">1");

        const auto ge = BuildNumberPredicate::make_greater_equal(1);
        REQUIRE_FALSE(ge.contains(0));
        REQUIRE(ge.contains(1));
        REQUIRE(ge.contains(2));
        CHECK_EQ(ge.str(), ">=1");

        const auto lt = BuildNumberPredicate::make_less(1);
        REQUIRE(lt.contains(0));
        REQUIRE_FALSE(lt.contains(1));
        REQUIRE_FALSE(lt.contains(2));
        CHECK_EQ(lt.str(), "<1");

        const auto le = BuildNumberPredicate::make_less_equal(1);
        REQUIRE(le.contains(0));
        REQUIRE(le.contains(1));
        REQUIRE_FALSE(le.contains(2));
        CHECK_EQ(le.str(), "<=1");

        const auto predicates = std::array{ free, eq, ne, lt, le, gt, ge };
        for (std::size_t i = 0; i < predicates.size(); ++i)
        {
            CHECK_EQ(predicates[i], predicates[i]);
            for (std::size_t j = i + 1; j < predicates.size(); ++j)
            {
                CHECK_NE(predicates[i], predicates[j]);
            }
        }
    }

    TEST_CASE("BuildNumberSepc::parse")
    {
        using namespace mamba::specs::build_number_spec_literals;

        SECTION("Successful")
        {
            REQUIRE(""_bs.contains(0));
            REQUIRE(""_bs.contains(1));
            REQUIRE("*"_bs.contains(1));
            REQUIRE("=*"_bs.contains(1));

            REQUIRE("1"_bs.contains(1));
            REQUIRE("=1"_bs.contains(1));
            REQUIRE_FALSE("1"_bs.contains(2));
            REQUIRE_FALSE("=1"_bs.contains(2));

            REQUIRE("!=1"_bs.contains(0));
            REQUIRE_FALSE("!=1"_bs.contains(1));
            REQUIRE("!=1"_bs.contains(2));

            REQUIRE_FALSE(">1"_bs.contains(0));
            REQUIRE_FALSE(">1"_bs.contains(1));
            REQUIRE(">1"_bs.contains(2));

            REQUIRE_FALSE(">=1"_bs.contains(0));
            REQUIRE(">=1"_bs.contains(1));
            REQUIRE(">=1"_bs.contains(2));

            REQUIRE("<1"_bs.contains(0));
            REQUIRE_FALSE("<1"_bs.contains(1));
            REQUIRE_FALSE("<1"_bs.contains(2));

            REQUIRE("<=1"_bs.contains(0));
            REQUIRE("<=1"_bs.contains(1));
            REQUIRE_FALSE("<=1"_bs.contains(2));

            REQUIRE(" <= 1 "_bs.contains(0));
        }

        SECTION("Unsuccessful")
        {
            using namespace std::literals::string_view_literals;

            static constexpr auto bad_specs = std::array{
                "<2.4"sv, "<"sv, "(3)"sv, "<2+"sv, "7=2+"sv, "@7"sv,
            };

            for (const auto& spec : bad_specs)
            {
                CAPTURE(spec);
                REQUIRE_FALSE(BuildNumberSpec::parse(spec).has_value());
            }
        }
    }

    TEST_CASE("BuildNumberSepc::str")
    {
        CHECK_EQ(BuildNumberSpec::parse("=3").value().str(), "=3");
        CHECK_EQ(BuildNumberSpec::parse("<2").value().str(), "<2");
        CHECK_EQ(BuildNumberSpec::parse("*").value().str(), "=*");
    }

    TEST_CASE("BuildNumberSepc::is_explicitly_free")
    {
        REQUIRE(BuildNumberSpec::parse("*").value().is_explicitly_free());
        REQUIRE_FALSE(BuildNumberSpec::parse("=3").value().is_explicitly_free());
        REQUIRE_FALSE(BuildNumberSpec::parse("<2").value().is_explicitly_free());
    }

    TEST_CASE("Comparability and hashability")
    {
        auto bn1 = BuildNumberSpec::parse("=3").value();
        auto bn2 = BuildNumberSpec::parse("3").value();
        auto bn3 = BuildNumberSpec::parse("*").value();

        CHECK_EQ(bn1, bn2);
        CHECK_NE(bn1, bn3);

        auto hash_fn = std::hash<BuildNumberSpec>{};
        auto bn1_hash = hash_fn(bn1);
        auto bn2_hash = hash_fn(bn2);
        auto bn3_hash = hash_fn(bn3);

        CHECK_EQ(bn1_hash, bn2_hash);
        CHECK_NE(bn1_hash, bn3_hash);
    }
}
