// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <stdexcept>
#include <string_view>

#include <doctest/doctest.h>

#include "mamba/specs/version_spec.hpp"

using namespace mamba::specs;

TEST_SUITE("version_spec")
{
    TEST_CASE("VersionInterval")
    {
        const auto v1 = Version::parse("1.0");
        const auto v2 = Version::parse("2.0");
        const auto v201 = Version::parse("2.0.1");
        const auto v3 = Version::parse("3.0");
        const auto v4 = Version::parse("4.0");

        const auto free = VersionPredicate::make_free();
        CHECK(free.contains(v1));
        CHECK(free.contains(v2));
        CHECK(free.contains(v3));
        CHECK(free.contains(v4));

        const auto eq = VersionPredicate::make_equal_to(v2);
        CHECK_FALSE(eq.contains(v1));
        CHECK(eq.contains(v2));
        CHECK_FALSE(eq.contains(v3));
        CHECK_FALSE(eq.contains(v4));

        const auto ne = VersionPredicate::make_not_equal_to(v2);
        CHECK(ne.contains(v1));
        CHECK_FALSE(ne.contains(v2));
        CHECK(ne.contains(v3));
        CHECK(ne.contains(v4));

        const auto gt = VersionPredicate::make_greater(v2);
        CHECK_FALSE(gt.contains(v1));
        CHECK_FALSE(gt.contains(v2));
        CHECK(gt.contains(v3));
        CHECK(gt.contains(v4));

        const auto ge = VersionPredicate::make_greater_equal(v2);
        CHECK_FALSE(ge.contains(v1));
        CHECK(ge.contains(v2));
        CHECK(ge.contains(v3));
        CHECK(ge.contains(v4));

        const auto lt = VersionPredicate::make_less(v2);
        CHECK(lt.contains(v1));
        CHECK_FALSE(lt.contains(v2));
        CHECK_FALSE(lt.contains(v3));
        CHECK_FALSE(lt.contains(v4));

        const auto le = VersionPredicate::make_less_equal(v2);
        CHECK(le.contains(v1));
        CHECK(le.contains(v2));
        CHECK_FALSE(le.contains(v3));
        CHECK_FALSE(le.contains(v4));

        const auto sw = VersionPredicate::make_starts_with(v2);
        CHECK_FALSE(sw.contains(v1));
        CHECK(sw.contains(v2));
        CHECK(sw.contains(v201));
        CHECK_FALSE(sw.contains(v3));
        CHECK_FALSE(sw.contains(v4));

        const auto cp2 = VersionPredicate::make_compatible_with(v2, 2);
        CHECK_FALSE(cp2.contains(v1));
        CHECK(cp2.contains(v2));
        CHECK(cp2.contains(v201));
        CHECK_FALSE(cp2.contains(v3));
        CHECK_FALSE(cp2.contains(v4));

        const auto cp3 = VersionPredicate::make_compatible_with(v2, 3);
        CHECK_FALSE(cp3.contains(v1));
        CHECK(cp3.contains(v2));
        CHECK_FALSE(cp3.contains(v201));
        CHECK_FALSE(cp3.contains(v3));
        CHECK_FALSE(cp3.contains(v4));

        const auto predicates = std::array{ free, eq, ne, lt, le, gt, ge, sw, cp2, cp3 };
        for (std::size_t i = 0; i < predicates.size(); ++i)
        {
            CHECK_EQ(predicates[i], predicates[i]);
            for (std::size_t j = i + 1; j < predicates.size(); ++j)
            {
                CHECK_NE(predicates[i], predicates[j]);
            }
        }
    }

    TEST_CASE("VersionInterval")
    {
        SUBCASE("empty")
        {
            auto spec = VersionSpec();
            CHECK(spec.contains(Version()));
        }

        SUBCASE("<2.0|(>2.3,<=2.8.0)")
        {
            using namespace mamba::util;

            const auto v20 = Version(0, { { { 2 } }, { { 0 } } });
            const auto v23 = Version(0, { { { 2 } }, { { 3 } } });
            const auto v28 = Version(0, { { { 2 } }, { { 8 } }, { { 0 } } });

            auto parser = InfixParser<VersionPredicate, BoolOperator>{};
            parser.push_variable(VersionPredicate::make_less(v20));
            parser.push_operator(BoolOperator::logical_or);
            parser.push_left_parenthesis();
            parser.push_variable(VersionPredicate::make_greater(v23));
            parser.push_operator(BoolOperator::logical_and);
            parser.push_variable(VersionPredicate::make_less_equal(v28));
            parser.push_right_parenthesis();
            parser.finalize();

            auto spec = VersionSpec(std::move(parser).tree());

            CHECK(spec.contains(Version(0, { { { 2 } }, { { 3 } }, { { 1 } } })));  // 2.3.1
            CHECK(spec.contains(Version(0, { { { 2 } }, { { 8 } } })));             // 2.8
            CHECK(spec.contains(Version(0, { { { 1 } }, { { 8 } } })));             // 1.8

            CHECK_FALSE(spec.contains(Version(0, { { { 2 } }, { { 0 } }, { { 0 } } })));  // 2.0.0
            CHECK_FALSE(spec.contains(Version(0, { { { 2 } }, { { 1 } } })));             // 2.1
            CHECK_FALSE(spec.contains(Version(0, { { { 2 } }, { { 3 } } })));             // 2.3
        }
    }

    TEST_CASE("Parsing")
    {
        using namespace std::literals::string_view_literals;

        SUBCASE("Successful")
        {
            // clang-format off
            static constexpr auto specs = std::array{
                "<1.7"sv,
                "<=1.7.0"sv,
                ">1.7.0 "sv,
                ">= 1.7"sv,
                "(>= 1.7, <1.8) |>=1.9.0.0"sv
            };
            static constexpr auto versions = std::array{
                "1.6"sv,
                "1.7.0.0"sv,
                "1.8.1"sv,
                "1.9.0"sv,
            };
            static constexpr auto contains = std::array{
                std::array{true, false, false, false},
                std::array{true, true, false, false},
                std::array{false, false, true, true},
                std::array{false, true, true, true},
                std::array{false, true, false, true},
            };
            // clang-format on

            for (std::size_t spec_id = 0; spec_id < specs.size(); ++spec_id)
            {
                CAPTURE(specs[spec_id]);
                const auto spec = VersionSpec::parse(specs[spec_id]);

                for (std::size_t ver_id = 0; ver_id < versions.size(); ++ver_id)
                {
                    CAPTURE(versions[ver_id]);
                    const auto ver = Version::parse(versions[ver_id]);
                    CHECK_EQ(spec.contains(ver), contains[spec_id][ver_id]);
                }
            }
        }
    }
}
