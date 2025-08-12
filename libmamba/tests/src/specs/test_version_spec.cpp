// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "mamba/specs/version_spec.hpp"

using namespace mamba::specs;

namespace
{
    using namespace mamba::specs::version_literals;
    using namespace mamba::specs::version_spec_literals;

    TEST_CASE("VersionPredicate", "[mamba::specs][mamba::specs::VersionSpec]")
    {
        const auto v1 = "1.0"_v;
        const auto v2 = "2.0"_v;
        const auto v201 = "2.0.1"_v;
        const auto v3 = "3.0"_v;
        const auto v4 = "4.0"_v;

        const auto free = VersionPredicate::make_free();
        REQUIRE(free.contains(v1));
        REQUIRE(free.contains(v2));
        REQUIRE(free.contains(v3));
        REQUIRE(free.contains(v4));
        REQUIRE(free.to_string() == "=*");
        REQUIRE_FALSE(free.has_glob());
        REQUIRE_FALSE(free.is_classic_operator());

        const auto eq = VersionPredicate::make_equal_to(v2);
        REQUIRE_FALSE(eq.contains(v1));
        REQUIRE(eq.contains(v2));
        REQUIRE_FALSE(eq.contains(v3));
        REQUIRE_FALSE(eq.contains(v4));
        REQUIRE(eq.to_string() == "==2.0");
        REQUIRE_FALSE(eq.has_glob());
        REQUIRE(eq.is_classic_operator());

        const auto ne = VersionPredicate::make_not_equal_to(v2);
        REQUIRE(ne.contains(v1));
        REQUIRE_FALSE(ne.contains(v2));
        REQUIRE(ne.contains(v3));
        REQUIRE(ne.contains(v4));
        REQUIRE(ne.to_string() == "!=2.0");
        REQUIRE_FALSE(ne.has_glob());
        REQUIRE(ne.is_classic_operator());

        const auto gt = VersionPredicate::make_greater(v2);
        REQUIRE_FALSE(gt.contains(v1));
        REQUIRE_FALSE(gt.contains(v2));
        REQUIRE(gt.contains(v3));
        REQUIRE(gt.contains(v4));
        REQUIRE(gt.to_string() == ">2.0");
        REQUIRE_FALSE(gt.has_glob());
        REQUIRE(gt.is_classic_operator());

        const auto ge = VersionPredicate::make_greater_equal(v2);
        REQUIRE_FALSE(ge.contains(v1));
        REQUIRE(ge.contains(v2));
        REQUIRE(ge.contains(v3));
        REQUIRE(ge.contains(v4));
        REQUIRE(ge.to_string() == ">=2.0");
        REQUIRE_FALSE(ge.has_glob());
        REQUIRE(ge.is_classic_operator());

        const auto lt = VersionPredicate::make_less(v2);
        REQUIRE(lt.contains(v1));
        REQUIRE_FALSE(lt.contains(v2));
        REQUIRE_FALSE(lt.contains(v3));
        REQUIRE_FALSE(lt.contains(v4));
        REQUIRE(lt.to_string() == "<2.0");
        REQUIRE_FALSE(lt.has_glob());
        REQUIRE(lt.is_classic_operator());

        const auto le = VersionPredicate::make_less_equal(v2);
        REQUIRE(le.contains(v1));
        REQUIRE(le.contains(v2));
        REQUIRE_FALSE(le.contains(v3));
        REQUIRE_FALSE(le.contains(v4));
        REQUIRE(le.to_string() == "<=2.0");
        REQUIRE_FALSE(le.has_glob());
        REQUIRE(le.is_classic_operator());

        const auto sw = VersionPredicate::make_starts_with(v2);
        REQUIRE_FALSE(sw.contains(v1));
        REQUIRE(sw.contains(v2));
        REQUIRE(sw.contains(v201));
        REQUIRE_FALSE(sw.contains(v3));
        REQUIRE_FALSE(sw.contains(v4));
        REQUIRE(sw.to_string() == "=2.0");
        REQUIRE(sw.to_string_conda_build() == "2.0.*");
        REQUIRE_FALSE(sw.has_glob());
        REQUIRE_FALSE(sw.is_classic_operator());

        const auto nsw = VersionPredicate::make_not_starts_with(v2);
        REQUIRE(nsw.contains(v1));
        REQUIRE_FALSE(nsw.contains(v2));
        REQUIRE_FALSE(nsw.contains(v201));
        REQUIRE(nsw.contains(v3));
        REQUIRE(nsw.contains(v4));
        REQUIRE(nsw.to_string() == "!=2.0.*");
        REQUIRE_FALSE(nsw.has_glob());
        REQUIRE_FALSE(nsw.is_classic_operator());

        const auto cp2 = VersionPredicate::make_compatible_with(v2, 2);
        REQUIRE_FALSE(cp2.contains(v1));
        REQUIRE(cp2.contains(v2));
        REQUIRE(cp2.contains(v201));
        REQUIRE_FALSE(cp2.contains(v3));
        REQUIRE_FALSE(cp2.contains(v4));
        REQUIRE(cp2.to_string() == "~=2.0");
        REQUIRE_FALSE(cp2.has_glob());
        REQUIRE_FALSE(cp2.is_classic_operator());

        const auto cp3 = VersionPredicate::make_compatible_with(v2, 3);
        REQUIRE_FALSE(cp3.contains(v1));
        REQUIRE(cp3.contains(v2));
        REQUIRE_FALSE(cp3.contains(v201));
        REQUIRE_FALSE(cp3.contains(v3));
        REQUIRE_FALSE(cp3.contains(v4));
        REQUIRE(cp3.to_string() == "~=2.0.0");
        REQUIRE_FALSE(cp3.is_classic_operator());

        const auto g1 = VersionPredicate::make_version_glob("*"_v);
        REQUIRE(g1.contains(v1));
        REQUIRE(g1.contains(v2));
        REQUIRE(g1.contains(v201));
        REQUIRE(g1.contains(v3));
        REQUIRE(g1.contains(v4));
        REQUIRE(g1.to_string() == "*");
        REQUIRE(g1.has_glob());
        REQUIRE_FALSE(g1.is_classic_operator());

        const auto g2 = VersionPredicate::make_version_glob("*.0.*"_v);
        REQUIRE_FALSE(g2.contains(v1));
        REQUIRE_FALSE(g2.contains(v2));
        REQUIRE(g2.contains(v201));
        REQUIRE_FALSE(g2.contains(v3));
        REQUIRE_FALSE(g2.contains(v4));
        REQUIRE(g2.contains("1.0.1.1.1"_v));
        REQUIRE(g2.to_string() == "*.0.*");

        const auto g3 = VersionPredicate::make_version_glob("*.0"_v);
        REQUIRE(g3.contains(v1));
        REQUIRE(g3.contains(v2));
        REQUIRE_FALSE(g3.contains(v201));
        REQUIRE(g3.contains(v3));
        REQUIRE(g3.contains(v4));
        REQUIRE(g3.to_string() == "*.0");

        const auto g4 = VersionPredicate::make_version_glob("2.*"_v);
        REQUIRE_FALSE(g4.contains(v1));
        REQUIRE(g4.contains(v2));
        REQUIRE(g4.contains(v201));
        REQUIRE_FALSE(g4.contains(v3));
        REQUIRE_FALSE(g4.contains(v4));
        REQUIRE(g4.to_string() == "2.*");

        const auto g5 = VersionPredicate::make_version_glob("2.0"_v);
        REQUIRE_FALSE(g5.contains(v1));
        REQUIRE(g5.contains(v2));
        REQUIRE_FALSE(g5.contains(v201));
        REQUIRE_FALSE(g5.contains(v3));
        REQUIRE_FALSE(g5.contains(v4));
        REQUIRE(g5.to_string() == "2.0");

        const auto g6 = VersionPredicate::make_version_glob("2.*.1"_v);
        REQUIRE_FALSE(g6.contains(v1));
        REQUIRE_FALSE(g6.contains(v2));
        REQUIRE(g6.contains(v201));
        REQUIRE_FALSE(g6.contains(v3));
        REQUIRE_FALSE(g6.contains(v4));
        REQUIRE(g6.to_string() == "2.*.1");

        const auto g7 = VersionPredicate::make_version_glob("2.*.1.1.*"_v);
        REQUIRE_FALSE(g7.contains(v1));
        REQUIRE_FALSE(g7.contains(v2));
        REQUIRE_FALSE(g7.contains(v201));
        REQUIRE(g7.contains("2.0.1.0.1.1.3"_v));
        REQUIRE(g7.to_string() == "2.*.1.1.*");

        const auto ng1 = VersionPredicate::make_not_version_glob("2.*.1"_v);
        REQUIRE(ng1.contains(v1));
        REQUIRE(ng1.contains(v2));
        REQUIRE_FALSE(ng1.contains(v201));
        REQUIRE(ng1.contains(v3));
        REQUIRE(ng1.contains(v4));
        REQUIRE(ng1.to_string() == "!=2.*.1");
        REQUIRE(ng1.has_glob());
        REQUIRE_FALSE(ng1.is_classic_operator());

        const auto predicates = std::array{
            free, eq, ne, lt, le, gt, ge, sw, cp2, cp3, g1, g2, g3, g4, g5, g6, g7, ng1,
        };
        REQUIRE("*.0"_v != "*.0.*"_v);
        for (std::size_t i = 0; i < predicates.size(); ++i)
        {
            REQUIRE(predicates[i] == predicates[i]);
            for (std::size_t j = i + 1; j < predicates.size(); ++j)
            {
                REQUIRE(predicates[i] != predicates[j]);
            }
        }
    }

    TEST_CASE("Tree construction", "[mamba::specs][mamba::specs::VersionSpec]")
    {
        SECTION("empty")
        {
            auto spec = VersionSpec();
            REQUIRE(spec.contains(Version()));
            REQUIRE(spec.to_string() == "=*");
        }

        SECTION("from_predicate")
        {
            const auto v1 = "1.0"_v;
            const auto v2 = "2.0"_v;
            auto spec = VersionSpec::from_predicate(VersionPredicate::make_equal_to(v1));
            REQUIRE(spec.contains(v1));
            REQUIRE_FALSE(spec.contains(v2));
            REQUIRE(spec.to_string() == "==1.0");
        }

        SECTION("<2.0|(>2.3,<=2.8.0)")
        {
            using namespace mamba::util;

            const auto v20 = Version(0, { { { 2 } }, { { 0 } } });
            const auto v23 = Version(0, { { { 2 } }, { { 3 } } });
            const auto v28 = Version(0, { { { 2 } }, { { 8 } }, { { 0 } } });

            auto parser = InfixParser<VersionPredicate, BoolOperator>{};
            REQUIRE(parser.push_variable(VersionPredicate::make_less(v20)));
            REQUIRE(parser.push_operator(BoolOperator::logical_or));
            REQUIRE(parser.push_left_parenthesis());
            REQUIRE(parser.push_variable(VersionPredicate::make_greater(v23)));
            REQUIRE(parser.push_operator(BoolOperator::logical_and));
            REQUIRE(parser.push_variable(VersionPredicate::make_less_equal(v28)));
            REQUIRE(parser.push_right_parenthesis());
            REQUIRE(parser.finalize());

            auto spec = VersionSpec(std::move(parser).tree());

            REQUIRE(spec.contains(Version(0, { { { 2 } }, { { 3 } }, { { 1 } } })));  // 2.3.1
            REQUIRE(spec.contains(Version(0, { { { 2 } }, { { 8 } } })));             // 2.8
            REQUIRE(spec.contains(Version(0, { { { 1 } }, { { 8 } } })));             // 1.8

            REQUIRE_FALSE(spec.contains(Version(0, { { { 2 } }, { { 0 } }, { { 0 } } })));  // 2.0.0
            REQUIRE_FALSE(spec.contains(Version(0, { { { 2 } }, { { 1 } } })));             // 2.1
            REQUIRE_FALSE(spec.contains(Version(0, { { { 2 } }, { { 3 } } })));             // 2.3

            // Note this won't always be the same as the parsed string because of the tree
            // serialization
            REQUIRE(spec.to_string() == "<2.0|(>2.3,<=2.8.0)");
        }
    }

    TEST_CASE("VersionSpec::parse", "[mamba::specs][mamba::specs::VersionSpec]")
    {
        SECTION("Successful")
        {
            REQUIRE(""_vs.contains("1.6"_v));
            REQUIRE(""_vs.contains("0.6+0.7"_v));

            REQUIRE("*"_vs.contains("1.4"_v));
            REQUIRE("=*"_vs.contains("1.4"_v));

            REQUIRE("1.7"_vs.contains("1.7"_v));
            REQUIRE("1.7"_vs.contains("1.7.0.0"_v));
            REQUIRE_FALSE("1.7"_vs.contains("1.6"_v));
            REQUIRE_FALSE("1.7"_vs.contains("1.7.7"_v));
            REQUIRE_FALSE("1.7"_vs.contains("1.7.0.1"_v));

            REQUIRE("==1.7"_vs.contains("1.7"_v));
            REQUIRE("==1.7"_vs.contains("1.7.0.0"_v));
            REQUIRE_FALSE("==1.7"_vs.contains("1.6"_v));
            REQUIRE_FALSE("==1.7"_vs.contains("1.7.7"_v));
            REQUIRE_FALSE("==1.7"_vs.contains("1.7.0.1"_v));

            REQUIRE_FALSE("!=1.7"_vs.contains("1.7"_v));
            REQUIRE_FALSE("!=1.7"_vs.contains("1.7.0.0"_v));
            REQUIRE("!=1.7"_vs.contains("1.6"_v));
            REQUIRE("!=1.7"_vs.contains("1.7.7"_v));
            REQUIRE("!=1.7"_vs.contains("1.7.0.1"_v));

            REQUIRE_FALSE("<1.7"_vs.contains("1.7"_v));
            REQUIRE_FALSE("<1.7"_vs.contains("1.7.0.0"_v));
            REQUIRE("<1.7"_vs.contains("1.6"_v));
            REQUIRE("<1.7"_vs.contains("1.7a"_v));
            REQUIRE_FALSE("<1.7"_vs.contains("1.7.7"_v));
            REQUIRE_FALSE("<1.7"_vs.contains("1.7.0.1"_v));

            REQUIRE("<=1.7"_vs.contains("1.7"_v));
            REQUIRE("<=1.7"_vs.contains("1.7.0.0"_v));
            REQUIRE("<=1.7"_vs.contains("1.6"_v));
            REQUIRE("<=1.7"_vs.contains("1.7a"_v));
            REQUIRE_FALSE("<=1.7"_vs.contains("1.7.7"_v));
            REQUIRE_FALSE("<=1.7"_vs.contains("1.7.0.1"_v));

            REQUIRE_FALSE(">1.7"_vs.contains("1.7"_v));
            REQUIRE_FALSE(">1.7"_vs.contains("1.7.0.0"_v));
            REQUIRE_FALSE(">1.7"_vs.contains("1.6"_v));
            REQUIRE_FALSE(">1.7"_vs.contains("1.7a"_v));
            REQUIRE(">1.7"_vs.contains("1.7.7"_v));
            REQUIRE(">1.7"_vs.contains("1.7.0.1"_v));

            REQUIRE(">= 1.7"_vs.contains("1.7"_v));
            REQUIRE(">= 1.7"_vs.contains("1.7.0.0"_v));
            REQUIRE_FALSE(">= 1.7"_vs.contains("1.6"_v));
            REQUIRE_FALSE(">= 1.7"_vs.contains("1.7a"_v));
            REQUIRE(">= 1.7"_vs.contains("1.7.7"_v));
            REQUIRE(">= 1.7"_vs.contains("1.7.0.1"_v));

            REQUIRE_FALSE(" = 1.8"_vs.contains("1.7.0.1"_v));
            REQUIRE(" = 1.8"_vs.contains("1.8"_v));
            REQUIRE(" = 1.8"_vs.contains("1.8.0"_v));
            REQUIRE(" = 1.8"_vs.contains("1.8.1"_v));
            REQUIRE(" = 1.8"_vs.contains("1.8alpha"_v));
            REQUIRE_FALSE(" = 1.8"_vs.contains("1.9"_v));

            REQUIRE_FALSE(" = 1.8.* "_vs.contains("1.7.0.1"_v));
            REQUIRE(" = 1.8.*"_vs.contains("1.8"_v));
            REQUIRE(" = 1.8.*"_vs.contains("1.8.0"_v));
            REQUIRE(" = 1.8.*"_vs.contains("1.8.1"_v));
            REQUIRE(" = 1.8.*"_vs.contains("1.8alpha"_v));  // Like Conda
            REQUIRE_FALSE(" = 1.8.*"_vs.contains("1.9"_v));

            REQUIRE_FALSE("  1.8.* "_vs.contains("1.7.0.1"_v));
            REQUIRE("  1.8.*"_vs.contains("1.8"_v));
            REQUIRE("  1.8.*"_vs.contains("1.8.0"_v));
            REQUIRE("  1.8.*"_vs.contains("1.8.1"_v));
            REQUIRE("  1.8.*"_vs.contains("1.8alpha"_v));  // Like Conda
            REQUIRE_FALSE("  1.8.*"_vs.contains("1.9"_v));

            REQUIRE(" != 1.8.*"_vs.contains("1.7.0.1"_v));
            REQUIRE_FALSE(" != 1.8.*"_vs.contains("1.8"_v));
            REQUIRE_FALSE(" != 1.8.*"_vs.contains("1.8.0"_v));
            REQUIRE_FALSE(" != 1.8.*"_vs.contains("1.8.1"_v));
            REQUIRE_FALSE(" != 1.8.*"_vs.contains("1.8alpha"_v));  // Like Conda
            REQUIRE(" != 1.8.*"_vs.contains("1.9"_v));

            REQUIRE(" 1.*.3"_vs.contains("1.7.3"_v));
            REQUIRE(" 1.*.3"_vs.contains("1.7.0.3"_v));
            REQUIRE_FALSE(" 1.*.3"_vs.contains("1.7.3.4"_v));
            REQUIRE_FALSE(" 1.*.3"_vs.contains("1.3"_v));
            REQUIRE_FALSE(" 1.*.3"_vs.contains("2.0.3"_v));

            REQUIRE(" =1.*.3"_vs.contains("1.7.3"_v));
            REQUIRE(" =1.*.3"_vs.contains("1.7.0.3"_v));
            REQUIRE_FALSE(" =1.*.3"_vs.contains("1.7.3.4"_v));
            REQUIRE_FALSE(" =1.*.3"_vs.contains("1.3"_v));
            REQUIRE_FALSE(" =1.*.3"_vs.contains("2.0.3"_v));

            REQUIRE_FALSE("!=1.*.3 "_vs.contains("1.7.3"_v));
            REQUIRE_FALSE("!=1.*.3 "_vs.contains("1.7.0.3"_v));
            REQUIRE("!=1.*.3 "_vs.contains("1.7.3.4"_v));
            REQUIRE("!=1.*.3 "_vs.contains("1.3"_v));
            REQUIRE("!=1.*.3 "_vs.contains("2.0.3"_v));

            REQUIRE_FALSE(" ~= 1.8 "_vs.contains("1.7.0.1"_v));
            REQUIRE(" ~= 1.8 "_vs.contains("1.8"_v));
            REQUIRE(" ~= 1.8 "_vs.contains("1.8.0"_v));
            REQUIRE(" ~= 1.8 "_vs.contains("1.8.1"_v));
            REQUIRE(" ~= 1.8 "_vs.contains("1.9"_v));
            REQUIRE(" ~= 1.8 "_vs.contains("1.8post"_v));
            REQUIRE_FALSE(" ~= 1.8 "_vs.contains("1.8alpha"_v));

            REQUIRE(" ~=1 "_vs.contains("1.7.0.1"_v));
            REQUIRE(" ~=1 "_vs.contains("1.8"_v));
            REQUIRE(" ~=1 "_vs.contains("1.8post"_v));
            REQUIRE(" ~=1 "_vs.contains("2.0"_v));
            REQUIRE_FALSE(" ~=1 "_vs.contains("0.1"_v));
            REQUIRE_FALSE(" ~=1 "_vs.contains("1.0.alpha"_v));

            REQUIRE_FALSE(" (>= 1.7, <1.8) |>=1.9.0.0 "_vs.contains("1.6"_v));
            REQUIRE(" (>= 1.7, <1.8) |>=1.9.0.0 "_vs.contains("1.7.0.0"_v));
            REQUIRE_FALSE(" (>= 1.7, <1.8) |>=1.9.0.0 "_vs.contains("1.8.1"_v));
            REQUIRE(" (>= 1.7, <1.8) |>=1.9.0.0 "_vs.contains("6.33"_v));

            // Test from Conda
            REQUIRE("==1.7"_vs.contains("1.7.0"_v));
            REQUIRE("<=1.7"_vs.contains("1.7.0"_v));
            REQUIRE_FALSE("<1.7"_vs.contains("1.7.0"_v));
            REQUIRE(">=1.7"_vs.contains("1.7.0"_v));
            REQUIRE_FALSE(">1.7"_vs.contains("1.7.0"_v));
            REQUIRE_FALSE(">=1.7"_vs.contains("1.6.7"_v));
            REQUIRE_FALSE(">2013b"_vs.contains("2013a"_v));
            REQUIRE(">2013b"_vs.contains("2013k"_v));
            REQUIRE_FALSE(">2013b"_vs.contains("3.0.0"_v));
            REQUIRE(">1.0.0a"_vs.contains("1.0.0"_v));
            REQUIRE(">1.0.0*"_vs.contains("1.0.0"_v));
            REQUIRE("1.0*"_vs.contains("1.0"_v));
            REQUIRE("1.0*"_vs.contains("1.0.0"_v));
            REQUIRE("1.0.0*"_vs.contains("1.0"_v));
            REQUIRE_FALSE("1.0.0*"_vs.contains("1.0.1"_v));
            REQUIRE("2013a*"_vs.contains("2013a"_v));
            REQUIRE_FALSE("2013b*"_vs.contains("2013a"_v));
            REQUIRE_FALSE("1.2.4*"_vs.contains("1.3.4"_v));
            REQUIRE("1.2.3*"_vs.contains("1.2.3+4.5.6"_v));
            REQUIRE("1.2.3+4*"_vs.contains("1.2.3+4.5.6"_v));
            REQUIRE_FALSE("1.2.3+5*"_vs.contains("1.2.3+4.5.6"_v));
            REQUIRE_FALSE("1.2.4+5*"_vs.contains("1.2.3+4.5.6"_v));
            REQUIRE("1.7.*"_vs.contains("1.7.1"_v));
            REQUIRE("1.7.1"_vs.contains("1.7.1"_v));
            REQUIRE_FALSE("1.7.0"_vs.contains("1.7.1"_v));
            REQUIRE_FALSE("1.7"_vs.contains("1.7.1"_v));
            REQUIRE_FALSE("1.5.*"_vs.contains("1.7.1"_v));
            REQUIRE(">=1.5"_vs.contains("1.7.1"_v));
            REQUIRE("!=1.5"_vs.contains("1.7.1"_v));
            REQUIRE_FALSE("!=1.7.1"_vs.contains("1.7.1"_v));
            REQUIRE("==1.7.1"_vs.contains("1.7.1"_v));
            REQUIRE_FALSE("==1.7"_vs.contains("1.7.1"_v));
            REQUIRE_FALSE("==1.7.2"_vs.contains("1.7.1"_v));
            REQUIRE("==1.7.1.0"_vs.contains("1.7.1"_v));
            REQUIRE("==1.7.1.*"_vs.contains("1.7.1.1"_v));  // Degenerate case
            REQUIRE("1.7.*|1.8.*"_vs.contains("1.7.1"_v));
            REQUIRE(">1.7,<1.8"_vs.contains("1.7.1"_v));
            REQUIRE_FALSE(">1.7.1,<1.8"_vs.contains("1.7.1"_v));
            REQUIRE("*"_vs.contains("1.7.1"_v));
            REQUIRE("1.5.*|>1.7,<1.8"_vs.contains("1.7.1"_v));
            REQUIRE_FALSE("1.5.*|>1.7,<1.7.1"_vs.contains("1.7.1"_v));
            REQUIRE("1.7.0.post123"_vs.contains("1.7.0.post123"_v));
            REQUIRE("1.7.0.post123.gabcdef9"_vs.contains("1.7.0.post123.gabcdef9"_v));
            REQUIRE("1.7.0.post123+gabcdef9"_vs.contains("1.7.0.post123+gabcdef9"_v));
            REQUIRE("=3.3"_vs.contains("3.3.1"_v));
            REQUIRE("=3.3"_vs.contains("3.3"_v));
            REQUIRE_FALSE("=3.3"_vs.contains("3.4"_v));
            REQUIRE("3.3.*"_vs.contains("3.3.1"_v));
            REQUIRE("3.3.*"_vs.contains("3.3"_v));
            REQUIRE_FALSE("3.3.*"_vs.contains("3.4"_v));
            REQUIRE("=3.3.*"_vs.contains("3.3.1"_v));
            REQUIRE("=3.3.*"_vs.contains("3.3"_v));
            REQUIRE_FALSE("=3.3.*"_vs.contains("3.4"_v));
            REQUIRE_FALSE("!=3.3.*"_vs.contains("3.3.1"_v));
            REQUIRE("!=3.3.*"_vs.contains("3.4"_v));
            REQUIRE("!=3.3.*"_vs.contains("3.4.1"_v));
            REQUIRE("!=3.3"_vs.contains("3.3.1"_v));
            REQUIRE_FALSE("!=3.3"_vs.contains("3.3.0.0"_v));
            REQUIRE_FALSE("!=3.3.*"_vs.contains("3.3.0.0"_v));
            REQUIRE_FALSE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("2.6.8"_v));
            REQUIRE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("2.7.2"_v));
            REQUIRE_FALSE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("3.3"_v));
            REQUIRE_FALSE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("3.3.4"_v));
            REQUIRE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("3.4"_v));
            REQUIRE(">=2.7, !=3.0.*, !=3.1.*, !=3.2.*, !=3.3.*"_vs.contains("3.4a"_v));
            REQUIRE("~=1.10"_vs.contains("1.11.0"_v));
            REQUIRE_FALSE("~=1.10.0"_vs.contains("1.11.0"_v));
            REQUIRE_FALSE("~=3.3.2"_vs.contains("3.4.0"_v));
            REQUIRE_FALSE("~=3.3.2"_vs.contains("3.3.1"_v));
            REQUIRE("~=3.3.2"_vs.contains("3.3.2.0"_v));
            REQUIRE("~=3.3.2"_vs.contains("3.3.3"_v));
            REQUIRE("~=3.3.2|==2.2"_vs.contains("2.2.0"_v));
            REQUIRE("~=3.3.2|==2.2"_vs.contains("3.3.3"_v));
            REQUIRE_FALSE("~=3.3.2|==2.2"_vs.contains("2.2.1"_v));
            REQUIRE("*.*"_vs.contains("3.3"_v));
            REQUIRE("*.*"_vs.contains("3.3.3"_v));
            REQUIRE_FALSE("*.*"_vs.contains("3"_v));
            REQUIRE("2.*.1.1.*"_vs.contains("2.0.1.0.1.1.3"_v));
            REQUIRE_FALSE("2.*.1.1.*"_vs.contains("2.1.0.1.1"_v));
            REQUIRE("*.3"_vs.contains("2.1.0.1.1.3"_v));
            REQUIRE_FALSE("*.3"_vs.contains("0.3.4"_v));
            REQUIRE("*.2023_10_12"_vs.contains("2.1.0.2023_10_12"_v));
            REQUIRE(">=10.0,*.*"_vs.contains("10.1"_v));
            REQUIRE_FALSE(">=10.0,*.*"_vs.contains("11"_v));
            REQUIRE("1.*.1"_vs.contains("1.7.1"_v));

            // Regex are currently not supported
            // REQUIRE("^1.7.1$"_vs.contains("1.7.1"_v));
            // REQUIRE(R"(^1\.7\.1$)"_vs.contains("1.7.1"_v));
            // REQUIRE(R"(^1\.7\.[0-9]+$)"_vs.contains("1.7.1"_v));
            // REQUIRE_FALSE(R"(^1\.8.*$)"_vs.contains("1.7.1"_v));
            // REQUIRE(R"(^1\.[5-8]\.1$)"_vs.contains("1.7.1"_v));
            // REQUIRE_FALSE(R"(^[^1].*$)"_vs.contains("1.7.1"_v));
            // REQUIRE(R"(^[0-9+]+\.[0-9+]+\.[0-9]+$)"_vs.contains("1.7.1"_v));
            // REQUIRE_FALSE("^$"_vs.contains("1.7.1"_v));
            // REQUIRE("^.*$"_vs.contains("1.7.1"_v));
            // REQUIRE("1.7.*|^0.*$"_vs.contains("1.7.1"_v));
            // REQUIRE_FALSE("1.6.*|^0.*$"_vs.contains("1.7.1"_v));
            // REQUIRE("1.6.*|^0.*$|1.7.1"_vs.contains("1.7.1"_v));
            // REQUIRE("^0.*$|1.7.1"_vs.contains("1.7.1"_v));
            // REQUIRE(R"(1.6.*|^.*\.7\.1$|0.7.1)"_vs.contains("1.7.1"_v));
        }

        SECTION("Unsuccessful")
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
                REQUIRE_FALSE(VersionSpec::parse(spec).has_value());
            }
        }
    }

    TEST_CASE("VersionSpec::str", "[mamba::specs][mamba::specs::VersionSpec]")
    {
        SECTION("2.3")
        {
            auto vs = VersionSpec::parse("2.3").value();
            REQUIRE(vs.to_string() == "==2.3");
            REQUIRE(vs.to_string_conda_build() == "==2.3");
        }

        SECTION("=2.3,<3.0")
        {
            auto vs = VersionSpec::parse("=2.3,<3.0").value();
            REQUIRE(vs.to_string() == "=2.3,<3.0");
            REQUIRE(vs.to_string_conda_build() == "2.3.*,<3.0");
        }

        SECTION("~=1")
        {
            auto vs = VersionSpec::parse("~=1").value();
            REQUIRE(vs.to_string() == "~=1");
            REQUIRE(vs.to_string_conda_build() == "~=1");
        }

        SECTION("~=1.8")
        {
            auto vs = VersionSpec::parse("~=1.8").value();
            REQUIRE(vs.to_string() == "~=1.8");
            REQUIRE(vs.to_string_conda_build() == "~=1.8");
        }

        SECTION("~=1.8.0")
        {
            auto vs = VersionSpec::parse("~=1.8.0").value();
            REQUIRE(vs.to_string() == "~=1.8.0");
            REQUIRE(vs.to_string_conda_build() == "~=1.8.0");
        }
    }

    TEST_CASE("VersionSpec::is_explicitly_free", "[mamba::specs][mamba::specs::VersionSpec]")
    {
        {
            using namespace mamba::util;

            auto parser = InfixParser<VersionPredicate, BoolOperator>{};
            REQUIRE(parser.push_variable(VersionPredicate::make_free()));
            REQUIRE(parser.finalize());
            auto spec = VersionSpec(std::move(parser).tree());

            REQUIRE(spec.is_explicitly_free());
        }

        REQUIRE(VersionSpec().is_explicitly_free());
        REQUIRE(VersionSpec::parse("*").value().is_explicitly_free());
        REQUIRE(VersionSpec::parse("").value().is_explicitly_free());

        REQUIRE_FALSE(VersionSpec::parse("==2.3|!=2.3").value().is_explicitly_free());
        REQUIRE_FALSE(VersionSpec::parse("=2.3,<3.0").value().is_explicitly_free());
    }

    TEST_CASE("VersionSpec::has_glob", "[mamba::specs][mamba::specs::VersionSpec]")
    {
        REQUIRE(VersionSpec::parse("*.4").value().has_glob());
        REQUIRE(VersionSpec::parse("1.*.0").value().has_glob());
        REQUIRE(VersionSpec::parse("1.0|4.*.0").value().has_glob());

        REQUIRE_FALSE(VersionSpec::parse("*").value().has_glob());
        REQUIRE_FALSE(VersionSpec::parse("3.*").value().has_glob());
    }

    TEST_CASE("VersionSpec Comparability and hashability", "[mamba::specs][mamba::specs::VersionSpec]")
    {
        auto spec1 = VersionSpec::parse("*").value();
        auto spec2 = VersionSpec::parse("*").value();
        auto spec3 = VersionSpec::parse("=2.4").value();

        REQUIRE(spec1 == spec2);
        REQUIRE(spec1 != spec3);

        auto hash_fn = std::hash<VersionSpec>();
        REQUIRE(hash_fn(spec1) == hash_fn(spec2));
        REQUIRE(hash_fn(spec1) != hash_fn(spec3));
    }
}
