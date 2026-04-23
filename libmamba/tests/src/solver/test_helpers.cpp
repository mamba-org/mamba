// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/version.hpp"

#include "solver/helpers.hpp"

using namespace mamba;
using namespace mamba::solver;

namespace
{
    auto parse_version(std::string_view s) -> specs::Version
    {
        return specs::Version::parse(s).value();
    }

    TEST_CASE("python_binary_compatible", "[mamba::solver]")
    {
        SECTION("identical versions are compatible")
        {
            const auto v = parse_version("3.11.14");
            REQUIRE(python_binary_compatible(v, v));
        }

        SECTION("same-minor micro upgrade is compatible")
        {
            // Regression test for the argument-order bug in
            // python_binary_compatible: the body used to call
            //   older.compatible_with(newer, 2)
            // but Version::compatible_with follows the convention
            //   newer.compatible_with(older, level)
            // (cf. libmamba/tests/src/specs/test_version.cpp).  The reversed
            // call reported every real Python upgrade as ABI-incompatible,
            // which caused solution_needs_python_relink() to emit a
            // Solution::Reinstall for every installed noarch:python package
            // on every micro upgrade.
            const auto older = parse_version("3.11.14");
            const auto newer = parse_version("3.11.15");
            REQUIRE(python_binary_compatible(older, newer));
        }

        SECTION("same-minor micro downgrade is compatible")
        {
            // Python ABI compatibility is symmetric within a minor series,
            // matching conda's get_major_minor_version equality check.
            const auto newer = parse_version("3.11.15");
            const auto older = parse_version("3.11.14");
            REQUIRE(python_binary_compatible(newer, older));
        }

        SECTION("different minor versions are not compatible")
        {
            const auto older = parse_version("3.11.14");
            const auto newer = parse_version("3.12.0");
            REQUIRE_FALSE(python_binary_compatible(older, newer));
        }

        SECTION("different major versions are not compatible")
        {
            const auto older = parse_version("2.7.18");
            const auto newer = parse_version("3.11.14");
            REQUIRE_FALSE(python_binary_compatible(older, newer));
        }
    }
}
