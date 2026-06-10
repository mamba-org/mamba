// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/exclude_newer.hpp"

using namespace mamba;

namespace
{
    TEST_CASE("resolve_exclude_newer_cutoff")
    {
        constexpr std::uint64_t now = 1'700'000'000;

        SECTION("empty value is disabled")
        {
            REQUIRE(resolve_exclude_newer_cutoff("", now) == std::nullopt);
            REQUIRE(resolve_exclude_newer_cutoff("   ", now) == std::nullopt);
        }

        SECTION("durations resolve relative to now")
        {
            REQUIRE(resolve_exclude_newer_cutoff("7d", now) == now - 7 * 86400);
            REQUIRE(resolve_exclude_newer_cutoff("P7D", now) == now - 7 * 86400);
            REQUIRE(resolve_exclude_newer_cutoff("3600", now) == now - 3600);
            REQUIRE(resolve_exclude_newer_cutoff("0d", now) == now);
            REQUIRE(resolve_exclude_newer_cutoff("P0D", now) == now);
        }

        SECTION("date-only values use the start of the next UTC day")
        {
            REQUIRE(resolve_exclude_newer_cutoff("2026-04-01", now) == 1'775'088'000);
        }

        SECTION("datetimes resolve to absolute instants")
        {
            REQUIRE(resolve_exclude_newer_cutoff("2026-04-01T12:00:00", now) == 1'775'044'800);
            REQUIRE(resolve_exclude_newer_cutoff("2026-04-01T10:00:00Z", now) == 1'775'037'600);
            REQUIRE(resolve_exclude_newer_cutoff("2026-04-01T12:00:00+02:00", now) == 1'775'037'600);
        }

        SECTION("invalid values throw")
        {
            REQUIRE_THROWS_AS(resolve_exclude_newer_cutoff("not-a-duration", now), std::invalid_argument);
            REQUIRE_THROWS_AS(resolve_exclude_newer_cutoff("P", now), std::invalid_argument);
        }
    }

    TEST_CASE("resolve_exclude_newer_package_cutoffs")
    {
        constexpr std::uint64_t now = 1'700'000'000;

        SECTION("false exempts a package")
        {
            const auto cutoffs = resolve_exclude_newer_package_cutoffs({ { "numpy", "false" } }, now);
            REQUIRE(cutoffs.at("numpy") == std::nullopt);
        }

        SECTION("package-specific durations override global semantics at resolve time")
        {
            const auto cutoffs = resolve_exclude_newer_package_cutoffs({ { "pandas", "7d" } }, now);
            REQUIRE(cutoffs.at("pandas") == now - 7 * 86400);
        }
    }

    TEST_CASE("ExcludeNewerCutoffPolicy")
    {
        constexpr std::uint64_t global_cutoff = 2000;

        const ExcludeNewerCutoffPolicy policy{
            /* .global= */ global_cutoff,
            /* .per_package= */
            {
                { "exempt-pkg", std::nullopt },
                { "custom-pkg", 1500 },
            },
        };

        SECTION("unknown packages use the global cutoff")
        {
            REQUIRE(policy.cutoff_for("other-pkg") == global_cutoff);
            REQUIRE(policy.excludes("other-pkg", 2500));
            REQUIRE_FALSE(policy.excludes("other-pkg", 1500));
        }

        SECTION("exempt packages ignore the global cutoff")
        {
            REQUIRE(policy.cutoff_for("exempt-pkg") == std::nullopt);
            REQUIRE_FALSE(policy.excludes("exempt-pkg", 999999));
        }

        SECTION("custom packages use their own cutoff")
        {
            REQUIRE(policy.cutoff_for("custom-pkg") == 1500);
            REQUIRE(policy.excludes("custom-pkg", 2000));
            REQUIRE_FALSE(policy.excludes("custom-pkg", 1000));
        }
    }
}  // namespace
