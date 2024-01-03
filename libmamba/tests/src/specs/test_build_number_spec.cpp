// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>

#include <doctest/doctest.h>

#include "mamba/specs/build_number_spec.hpp"

using namespace mamba::specs;

TEST_SUITE("specs::build_number_spec")
{
    TEST_CASE("BuildNumberPredicate")
    {
        const auto free = BuildNumberPredicate::make_free();
        CHECK(free.contains(0));
        CHECK(free.contains(1));
        CHECK(free.contains(2));
        CHECK_EQ(free.str(), "=*");

        const auto eq = BuildNumberPredicate::make_equal_to(1);
        CHECK_FALSE(eq.contains(0));
        CHECK(eq.contains(1));
        CHECK_FALSE(eq.contains(2));
        CHECK_EQ(eq.str(), "=1");

        const auto ne = BuildNumberPredicate::make_not_equal_to(1);
        CHECK(ne.contains(0));
        CHECK_FALSE(ne.contains(1));
        CHECK(ne.contains(2));
        CHECK_EQ(ne.str(), "!=1");

        const auto gt = BuildNumberPredicate::make_greater(1);
        CHECK_FALSE(gt.contains(0));
        CHECK_FALSE(gt.contains(1));
        CHECK(gt.contains(2));
        CHECK_EQ(gt.str(), ">1");

        const auto ge = BuildNumberPredicate::make_greater_equal(1);
        CHECK_FALSE(ge.contains(0));
        CHECK(ge.contains(1));
        CHECK(ge.contains(2));
        CHECK_EQ(ge.str(), ">=1");

        const auto lt = BuildNumberPredicate::make_less(1);
        CHECK(lt.contains(0));
        CHECK_FALSE(lt.contains(1));
        CHECK_FALSE(lt.contains(2));
        CHECK_EQ(lt.str(), "<1");

        const auto le = BuildNumberPredicate::make_less_equal(1);
        CHECK(le.contains(0));
        CHECK(le.contains(1));
        CHECK_FALSE(le.contains(2));
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

        SUBCASE("Successful")
        {
            CHECK(""_bs.contains(0));
            CHECK(""_bs.contains(1));
            CHECK("*"_bs.contains(1));
            CHECK("=*"_bs.contains(1));

            CHECK("1"_bs.contains(1));
            CHECK("=1"_bs.contains(1));
            CHECK_FALSE("1"_bs.contains(2));
            CHECK_FALSE("=1"_bs.contains(2));

            CHECK("!=1"_bs.contains(0));
            CHECK_FALSE("!=1"_bs.contains(1));
            CHECK("!=1"_bs.contains(2));

            CHECK_FALSE(">1"_bs.contains(0));
            CHECK_FALSE(">1"_bs.contains(1));
            CHECK(">1"_bs.contains(2));

            CHECK_FALSE(">=1"_bs.contains(0));
            CHECK(">=1"_bs.contains(1));
            CHECK(">=1"_bs.contains(2));

            CHECK("<1"_bs.contains(0));
            CHECK_FALSE("<1"_bs.contains(1));
            CHECK_FALSE("<1"_bs.contains(2));

            CHECK("<=1"_bs.contains(0));
            CHECK("<=1"_bs.contains(1));
            CHECK_FALSE("<=1"_bs.contains(2));
        }

        SUBCASE("Unsuccessful")
        {
            using namespace std::literals::string_view_literals;

            static constexpr auto bad_specs = std::array{
                "<2.4"sv, "<"sv, "(3)"sv, "<2+"sv, "7=2+"sv, "@7"sv,
            };

            for (const auto& spec : bad_specs)
            {
                CAPTURE(spec);
                // Silence [[nodiscard]] warning
                auto parse = [](auto s) { return BuildNumberSpec::parse(s); };
                CHECK_THROWS_AS(parse(spec), std::invalid_argument);
            }
        }
    }

    TEST_CASE("BuildNumberSepc::str")
    {
        CHECK_EQ(BuildNumberSpec::parse("=3").str(), "=3");
        CHECK_EQ(BuildNumberSpec::parse("<2").str(), "<2");
        CHECK_EQ(BuildNumberSpec::parse("*").str(), "=*");
    }

    TEST_CASE("BuildNumberSepc::is_explicitly_free")
    {
        CHECK(BuildNumberSpec::parse("*").is_explicitly_free());
        CHECK_FALSE(BuildNumberSpec::parse("=3").is_explicitly_free());
        CHECK_FALSE(BuildNumberSpec::parse("<2").is_explicitly_free());
    }
}
