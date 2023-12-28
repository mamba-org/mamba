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

TEST_SUITE("specs::version_spec")
{
    TEST_CASE("VersionPredicate")
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
        CHECK_EQ(free.str(), "=*");

        const auto eq = VersionPredicate::make_equal_to(v2);
        CHECK_FALSE(eq.contains(v1));
        CHECK(eq.contains(v2));
        CHECK_FALSE(eq.contains(v3));
        CHECK_FALSE(eq.contains(v4));
        CHECK_EQ(eq.str(), "==2.0");

        const auto ne = VersionPredicate::make_not_equal_to(v2);
        CHECK(ne.contains(v1));
        CHECK_FALSE(ne.contains(v2));
        CHECK(ne.contains(v3));
        CHECK(ne.contains(v4));
        CHECK_EQ(ne.str(), "!=2.0");

        const auto gt = VersionPredicate::make_greater(v2);
        CHECK_FALSE(gt.contains(v1));
        CHECK_FALSE(gt.contains(v2));
        CHECK(gt.contains(v3));
        CHECK(gt.contains(v4));
        CHECK_EQ(gt.str(), ">2.0");

        const auto ge = VersionPredicate::make_greater_equal(v2);
        CHECK_FALSE(ge.contains(v1));
        CHECK(ge.contains(v2));
        CHECK(ge.contains(v3));
        CHECK(ge.contains(v4));
        CHECK_EQ(ge.str(), ">=2.0");

        const auto lt = VersionPredicate::make_less(v2);
        CHECK(lt.contains(v1));
        CHECK_FALSE(lt.contains(v2));
        CHECK_FALSE(lt.contains(v3));
        CHECK_FALSE(lt.contains(v4));
        CHECK_EQ(lt.str(), "<2.0");

        const auto le = VersionPredicate::make_less_equal(v2);
        CHECK(le.contains(v1));
        CHECK(le.contains(v2));
        CHECK_FALSE(le.contains(v3));
        CHECK_FALSE(le.contains(v4));
        CHECK_EQ(le.str(), "<=2.0");

        const auto sw = VersionPredicate::make_starts_with(v2);
        CHECK_FALSE(sw.contains(v1));
        CHECK(sw.contains(v2));
        CHECK(sw.contains(v201));
        CHECK_FALSE(sw.contains(v3));
        CHECK_FALSE(sw.contains(v4));
        CHECK_EQ(sw.str(), "=2.0");
        CHECK_EQ(sw.str_conda_build(), "2.0.*");

        const auto nsw = VersionPredicate::make_not_starts_with(v2);
        CHECK(nsw.contains(v1));
        CHECK_FALSE(nsw.contains(v2));
        CHECK_FALSE(nsw.contains(v201));
        CHECK(nsw.contains(v3));
        CHECK(nsw.contains(v4));
        CHECK_EQ(nsw.str(), "!=2.0.*");

        const auto cp2 = VersionPredicate::make_compatible_with(v2, 2);
        CHECK_FALSE(cp2.contains(v1));
        CHECK(cp2.contains(v2));
        CHECK(cp2.contains(v201));
        CHECK_FALSE(cp2.contains(v3));
        CHECK_FALSE(cp2.contains(v4));
        CHECK_EQ(cp2.str(), "~=2.0");

        const auto cp3 = VersionPredicate::make_compatible_with(v2, 3);
        CHECK_FALSE(cp3.contains(v1));
        CHECK(cp3.contains(v2));
        CHECK_FALSE(cp3.contains(v201));
        CHECK_FALSE(cp3.contains(v3));
        CHECK_FALSE(cp3.contains(v4));
        CHECK_EQ(cp3.str(), "~=2.0.0");

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

    TEST_CASE("Tree construction")
    {
        SUBCASE("empty")
        {
            auto spec = VersionSpec();
            CHECK(spec.contains(Version()));
            CHECK_EQ(spec.str(), "=*");
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

            // Note this won't always be the same as the parsed string because of the tree
            // serialization
            CHECK_EQ(spec.str(), "<2.0|(>2.3,<=2.8.0)");
        }
    }

    TEST_CASE("Parsing")
    {
        using namespace mamba::specs::version_literals;
        using namespace mamba::specs::version_spec_literals;

        SUBCASE("Successful")
        {
            CHECK(""_vs.contains("1.6"_v));
            CHECK(""_vs.contains("0.6+0.7"_v));

            CHECK("*"_vs.contains("1.4"_v));
            CHECK("=*"_vs.contains("1.4"_v));

            CHECK("1.7"_vs.contains("1.7"_v));
            CHECK("1.7"_vs.contains("1.7.0.0"_v));
            CHECK_FALSE("1.7"_vs.contains("1.6"_v));
            CHECK_FALSE("1.7"_vs.contains("1.7.7"_v));
            CHECK_FALSE("1.7"_vs.contains("1.7.0.1"_v));

            CHECK("==1.7"_vs.contains("1.7"_v));
            CHECK("==1.7"_vs.contains("1.7.0.0"_v));
            CHECK_FALSE("==1.7"_vs.contains("1.6"_v));
            CHECK_FALSE("==1.7"_vs.contains("1.7.7"_v));
            CHECK_FALSE("==1.7"_vs.contains("1.7.0.1"_v));

            CHECK_FALSE("!=1.7"_vs.contains("1.7"_v));
            CHECK_FALSE("!=1.7"_vs.contains("1.7.0.0"_v));
            CHECK("!=1.7"_vs.contains("1.6"_v));
            CHECK("!=1.7"_vs.contains("1.7.7"_v));
            CHECK("!=1.7"_vs.contains("1.7.0.1"_v));

            CHECK_FALSE("<1.7"_vs.contains("1.7"_v));
            CHECK_FALSE("<1.7"_vs.contains("1.7.0.0"_v));
            CHECK("<1.7"_vs.contains("1.6"_v));
            CHECK("<1.7"_vs.contains("1.7a"_v));
            CHECK_FALSE("<1.7"_vs.contains("1.7.7"_v));
            CHECK_FALSE("<1.7"_vs.contains("1.7.0.1"_v));

            CHECK("<=1.7"_vs.contains("1.7"_v));
            CHECK("<=1.7"_vs.contains("1.7.0.0"_v));
            CHECK("<=1.7"_vs.contains("1.6"_v));
            CHECK("<=1.7"_vs.contains("1.7a"_v));
            CHECK_FALSE("<=1.7"_vs.contains("1.7.7"_v));
            CHECK_FALSE("<=1.7"_vs.contains("1.7.0.1"_v));

            CHECK_FALSE(">1.7"_vs.contains("1.7"_v));
            CHECK_FALSE(">1.7"_vs.contains("1.7.0.0"_v));
            CHECK_FALSE(">1.7"_vs.contains("1.6"_v));
            CHECK_FALSE(">1.7"_vs.contains("1.7a"_v));
            CHECK(">1.7"_vs.contains("1.7.7"_v));
            CHECK(">1.7"_vs.contains("1.7.0.1"_v));

            CHECK(">= 1.7"_vs.contains("1.7"_v));
            CHECK(">= 1.7"_vs.contains("1.7.0.0"_v));
            CHECK_FALSE(">= 1.7"_vs.contains("1.6"_v));
            CHECK_FALSE(">= 1.7"_vs.contains("1.7a"_v));
            CHECK(">= 1.7"_vs.contains("1.7.7"_v));
            CHECK(">= 1.7"_vs.contains("1.7.0.1"_v));

            CHECK_FALSE(" = 1.8"_vs.contains("1.7.0.1"_v));
            CHECK(" = 1.8"_vs.contains("1.8"_v));
            CHECK(" = 1.8"_vs.contains("1.8.0"_v));
            CHECK(" = 1.8"_vs.contains("1.8.1"_v));
            CHECK(" = 1.8"_vs.contains("1.8alpha"_v));
            CHECK_FALSE(" = 1.8"_vs.contains("1.9"_v));

            CHECK_FALSE(" = 1.8.* "_vs.contains("1.7.0.1"_v));
            CHECK(" = 1.8.*"_vs.contains("1.8"_v));
            CHECK(" = 1.8.*"_vs.contains("1.8.0"_v));
            CHECK(" = 1.8.*"_vs.contains("1.8.1"_v));
            CHECK(" = 1.8.*"_vs.contains("1.8alpha"_v));  // Like Conda
            CHECK_FALSE(" = 1.8.*"_vs.contains("1.9"_v));

            CHECK_FALSE("  1.8.* "_vs.contains("1.7.0.1"_v));
            CHECK("  1.8.*"_vs.contains("1.8"_v));
            CHECK("  1.8.*"_vs.contains("1.8.0"_v));
            CHECK("  1.8.*"_vs.contains("1.8.1"_v));
            CHECK("  1.8.*"_vs.contains("1.8alpha"_v));  // Like Conda
            CHECK_FALSE("  1.8.*"_vs.contains("1.9"_v));

            CHECK(" != 1.8.*"_vs.contains("1.7.0.1"_v));
            CHECK_FALSE(" != 1.8.*"_vs.contains("1.8"_v));
            CHECK_FALSE(" != 1.8.*"_vs.contains("1.8.0"_v));
            CHECK_FALSE(" != 1.8.*"_vs.contains("1.8.1"_v));
            CHECK_FALSE(" != 1.8.*"_vs.contains("1.8alpha"_v));  // Like Conda
            CHECK(" != 1.8.*"_vs.contains("1.9"_v));

            CHECK_FALSE(" ~= 1.8 "_vs.contains("1.7.0.1"_v));
            CHECK(" ~= 1.8 "_vs.contains("1.8"_v));
            CHECK(" ~= 1.8 "_vs.contains("1.8.0"_v));
            CHECK(" ~= 1.8 "_vs.contains("1.8.1"_v));
            CHECK(" ~= 1.8 "_vs.contains("1.9"_v));
            CHECK(" ~= 1.8 "_vs.contains("1.8post"_v));
            CHECK_FALSE(" ~= 1.8 "_vs.contains("1.8alpha"_v));

            CHECK(" ~=1 "_vs.contains("1.7.0.1"_v));
            CHECK(" ~=1 "_vs.contains("1.8"_v));
            CHECK(" ~=1 "_vs.contains("1.8post"_v));
            CHECK(" ~=1 "_vs.contains("2.0"_v));
            CHECK_FALSE(" ~=1 "_vs.contains("0.1"_v));
            CHECK_FALSE(" ~=1 "_vs.contains("1.0.alpha"_v));

            CHECK_FALSE(" (>= 1.7, <1.8) |>=1.9.0.0 "_vs.contains("1.6"_v));
            CHECK(" (>= 1.7, <1.8) |>=1.9.0.0 "_vs.contains("1.7.0.0"_v));
            CHECK_FALSE(" (>= 1.7, <1.8) |>=1.9.0.0 "_vs.contains("1.8.1"_v));
            CHECK(" (>= 1.7, <1.8) |>=1.9.0.0 "_vs.contains("6.33"_v));

            // Test from Conda
            CHECK("==1.7"_vs.contains("1.7.0"_v));
            CHECK("<=1.7"_vs.contains("1.7.0"_v));
            CHECK_FALSE("<1.7"_vs.contains("1.7.0"_v));
            CHECK(">=1.7"_vs.contains("1.7.0"_v));
            CHECK_FALSE(">1.7"_vs.contains("1.7.0"_v));
            CHECK_FALSE(">=1.7"_vs.contains("1.6.7"_v));
            CHECK_FALSE(">2013b"_vs.contains("2013a"_v));
            CHECK(">2013b"_vs.contains("2013k"_v));
            CHECK_FALSE(">2013b"_vs.contains("3.0.0"_v));
            CHECK(">1.0.0a"_vs.contains("1.0.0"_v));
            CHECK(">1.0.0*"_vs.contains("1.0.0"_v));
            CHECK("1.0*"_vs.contains("1.0"_v));
            CHECK("1.0*"_vs.contains("1.0.0"_v));
            CHECK("1.0.0*"_vs.contains("1.0"_v));
            CHECK_FALSE("1.0.0*"_vs.contains("1.0.1"_v));
            CHECK("2013a*"_vs.contains("2013a"_v));
            CHECK_FALSE("2013b*"_vs.contains("2013a"_v));
            CHECK_FALSE("1.2.4*"_vs.contains("1.3.4"_v));
            CHECK("1.2.3*"_vs.contains("1.2.3+4.5.6"_v));
            CHECK("1.2.3+4*"_vs.contains("1.2.3+4.5.6"_v));
            CHECK_FALSE("1.2.3+5*"_vs.contains("1.2.3+4.5.6"_v));
            CHECK_FALSE("1.2.4+5*"_vs.contains("1.2.3+4.5.6"_v));
            CHECK("1.7.*"_vs.contains("1.7.1"_v));
            CHECK("1.7.1"_vs.contains("1.7.1"_v));
            CHECK_FALSE("1.7.0"_vs.contains("1.7.1"_v));
            CHECK_FALSE("1.7"_vs.contains("1.7.1"_v));
            CHECK_FALSE("1.5.*"_vs.contains("1.7.1"_v));
            CHECK(">=1.5"_vs.contains("1.7.1"_v));
            CHECK("!=1.5"_vs.contains("1.7.1"_v));
            CHECK_FALSE("!=1.7.1"_vs.contains("1.7.1"_v));
            CHECK("==1.7.1"_vs.contains("1.7.1"_v));
            CHECK_FALSE("==1.7"_vs.contains("1.7.1"_v));
            CHECK_FALSE("==1.7.2"_vs.contains("1.7.1"_v));
            CHECK("==1.7.1.0"_vs.contains("1.7.1"_v));
            CHECK("1.7.*|1.8.*"_vs.contains("1.7.1"_v));
            CHECK(">1.7,<1.8"_vs.contains("1.7.1"_v));
            CHECK_FALSE(">1.7.1,<1.8"_vs.contains("1.7.1"_v));
            CHECK("*"_vs.contains("1.7.1"_v));
            CHECK("1.5.*|>1.7,<1.8"_vs.contains("1.7.1"_v));
            CHECK_FALSE("1.5.*|>1.7,<1.7.1"_vs.contains("1.7.1"_v));
            CHECK("1.7.0.post123"_vs.contains("1.7.0.post123"_v));
            CHECK("1.7.0.post123.gabcdef9"_vs.contains("1.7.0.post123.gabcdef9"_v));
            CHECK("1.7.0.post123+gabcdef9"_vs.contains("1.7.0.post123+gabcdef9"_v));
            CHECK("=3.3"_vs.contains("3.3.1"_v));
            CHECK("=3.3"_vs.contains("3.3"_v));
            CHECK_FALSE("=3.3"_vs.contains("3.4"_v));
            CHECK("3.3.*"_vs.contains("3.3.1"_v));
            CHECK("3.3.*"_vs.contains("3.3"_v));
            CHECK_FALSE("3.3.*"_vs.contains("3.4"_v));
            CHECK("=3.3.*"_vs.contains("3.3.1"_v));
            CHECK("=3.3.*"_vs.contains("3.3"_v));
            CHECK_FALSE("=3.3.*"_vs.contains("3.4"_v));
            CHECK_FALSE("!=3.3.*"_vs.contains("3.3.1"_v));
            CHECK("!=3.3.*"_vs.contains("3.4"_v));
            CHECK("!=3.3.*"_vs.contains("3.4.1"_v));
            CHECK("!=3.3"_vs.contains("3.3.1"_v));
            CHECK_FALSE("!=3.3"_vs.contains("3.3.0.0"_v));
            CHECK_FALSE("!=3.3.*"_vs.contains("3.3.0.0"_v));
            CHECK_FALSE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("2.6.8"_v));
            CHECK(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("2.7.2"_v));
            CHECK_FALSE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("3.3"_v));
            CHECK_FALSE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("3.3.4"_v));
            CHECK(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("3.4"_v));
            CHECK(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("3.4a"_v));
            CHECK("~=1.10"_vs.contains("1.11.0"_v));
            CHECK_FALSE("~=1.10.0"_vs.contains("1.11.0"_v));
            CHECK_FALSE("~=3.3.2"_vs.contains("3.4.0"_v));
            CHECK_FALSE("~=3.3.2"_vs.contains("3.3.1"_v));
            CHECK("~=3.3.2"_vs.contains("3.3.2.0"_v));
            CHECK("~=3.3.2"_vs.contains("3.3.3"_v));
            CHECK("~=3.3.2|==2.2"_vs.contains("2.2.0"_v));
            CHECK("~=3.3.2|==2.2"_vs.contains("3.3.3"_v));
            CHECK_FALSE("~=3.3.2|==2.2"_vs.contains("2.2.1"_v));

            // Regex are currently not supported
            // CHECK("^1.7.1$"_vs.contains("1.7.1"_v));
            // CHECK(R"(^1\.7\.1$)"_vs.contains("1.7.1"_v));
            // CHECK(R"(^1\.7\.[0-9]+$)"_vs.contains("1.7.1"_v));
            // CHECK_FALSE(R"(^1\.8.*$)"_vs.contains("1.7.1"_v));
            // CHECK(R"(^1\.[5-8]\.1$)"_vs.contains("1.7.1"_v));
            // CHECK_FALSE(R"(^[^1].*$)"_vs.contains("1.7.1"_v));
            // CHECK(R"(^[0-9+]+\.[0-9+]+\.[0-9]+$)"_vs.contains("1.7.1"_v));
            // CHECK_FALSE("^$"_vs.contains("1.7.1"_v));
            // CHECK("^.*$"_vs.contains("1.7.1"_v));
            // CHECK("1.7.*|^0.*$"_vs.contains("1.7.1"_v));
            // CHECK_FALSE("1.6.*|^0.*$"_vs.contains("1.7.1"_v));
            // CHECK("1.6.*|^0.*$|1.7.1"_vs.contains("1.7.1"_v));
            // CHECK("^0.*$|1.7.1"_vs.contains("1.7.1"_v));
            // CHECK(R"(1.6.*|^.*\.7\.1$|0.7.1)"_vs.contains("1.7.1"_v));
            // CHECK("1.*.1"_vs.contains("1.7.1"_v));
        }

        SUBCASE("Unsuccesful")
        {
            using namespace std::literals::string_view_literals;
            static constexpr auto bad_specs = std::array{
                "><2.4.5"sv,
                "!!2.4.5"sv,
                "!"sv,
                "(1.5"sv,
                "1.5)"sv,
                "1.5||1.6"sv,
                "^1.5"sv,
                "~"sv,
                "^"sv,
                "===3.3.2"sv,  // PEP440 arbitrary equality not implemented in Conda
                "~=3.3.2.*"sv
                // Conda tests
                "1.2+"sv,
                "+1.2"sv,
                "+1.2+"sv,
                "++"sv,
                "c +, 0/|0 *"sv,
                "a[version=)|("sv,
                "a=)(=b"sv,
                "=="sv,
                "="sv,
                ">="sv,
                "<="sv,
            };

            for (const auto& spec : bad_specs)
            {
                CAPTURE(spec);
                // Silence [[nodiscard]] warning
                auto parse = [](auto s) { return VersionSpec::parse(s); };
                CHECK_THROWS_AS(parse(spec), std::invalid_argument);
            }
        }
    }

    TEST_CASE("VersionSpec::str")
    {
        SUBCASE("2.3")
        {
            auto vs = VersionSpec::parse("2.3");
            CHECK_EQ(vs.str(), "==2.3");
            CHECK_EQ(vs.str_conda_build(), "==2.3");
        }

        SUBCASE("=2.3,<3.0")
        {
            auto vs = VersionSpec::parse("=2.3,<3.0");
            CHECK_EQ(vs.str(), "=2.3,<3.0");
            CHECK_EQ(vs.str_conda_build(), "2.3.*,<3.0");
        }
    }

    TEST_CASE("VersionSpec::is_free")
    {
        {
            using namespace mamba::util;

            auto parser = InfixParser<VersionPredicate, BoolOperator>{};
            parser.push_variable(VersionPredicate::make_free());
            parser.finalize();
            auto spec = VersionSpec(std::move(parser).tree());

            CHECK(spec.is_explicitly_free());
        }

        CHECK(VersionSpec().is_explicitly_free());
        CHECK(VersionSpec::parse("*").is_explicitly_free());
        CHECK(VersionSpec::parse("").is_explicitly_free());

        CHECK_FALSE(VersionSpec::parse("==2.3|!=2.3").is_explicitly_free());
        CHECK_FALSE(VersionSpec::parse("=2.3,<3.0").is_explicitly_free());
    }
}
