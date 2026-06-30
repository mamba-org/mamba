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

        SECTION("empty or whitespace-only values disable the policy")
        {
            REQUIRE(resolve_exclude_newer_cutoff("", now) == std::nullopt);
            REQUIRE(resolve_exclude_newer_cutoff("   ", now) == std::nullopt);
        }

        SECTION("zero values use the current time as the cutoff")
        {
            REQUIRE(resolve_exclude_newer_cutoff("0", now) == now);
            REQUIRE(resolve_exclude_newer_cutoff("0d", now) == now);
            REQUIRE(resolve_exclude_newer_cutoff("P0D", now) == now);
        }

        SECTION("plain integers are durations in seconds")
        {
            REQUIRE(resolve_exclude_newer_cutoff("3600", now) == now - 3600);
        }

        SECTION("compact durations resolve relative to now")
        {
            REQUIRE(resolve_exclude_newer_cutoff("7d", now) == now - 7 * 86400);
            REQUIRE(resolve_exclude_newer_cutoff("1w", now) == now - 604800);
            REQUIRE(resolve_exclude_newer_cutoff("3d12h", now) == now - (3 * 86400 + 12 * 3600));
        }

        SECTION("ISO 8601 durations resolve relative to now")
        {
            REQUIRE(resolve_exclude_newer_cutoff("P7D", now) == now - 7 * 86400);
            REQUIRE(resolve_exclude_newer_cutoff("PT24H", now) == now - 24 * 3600);
            REQUIRE(resolve_exclude_newer_cutoff("P1DT12H", now) == now - (86400 + 12 * 3600));
        }

        SECTION("date-only values use the start of the next UTC day")
        {
            REQUIRE(resolve_exclude_newer_cutoff("2026-04-01", now) == 1'775'088'000);
        }

        SECTION("date-only values roll over at month and year boundaries")
        {
            REQUIRE(resolve_exclude_newer_cutoff("2026-01-31", now) == 1'769'904'000);  // 2026-02-01
            REQUIRE(resolve_exclude_newer_cutoff("2025-02-28", now) == 1'740'787'200);  // 2025-03-01
            REQUIRE(resolve_exclude_newer_cutoff("2024-02-29", now) == 1'709'251'200);  // 2024-03-01
            REQUIRE(resolve_exclude_newer_cutoff("2025-12-31", now) == 1'767'225'600);  // 2026-01-01
        }

        SECTION("RFC 3339 datetimes resolve to absolute UTC instants")
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

    TEST_CASE("ExcludeNewerPolicy cutoff behavior")
    {
        constexpr std::uint64_t global_cutoff = 2000;

        const ExcludeNewerPolicy policy{
            /* .exclude_newer= */ "",
            /* .exclude_newer_package= */ {},
            /* .global= */ global_cutoff,
            /* .per_package= */
            {
                { "exempt-pkg", std::nullopt },
                { "custom-pkg", 1500 },
            },
        };

        SECTION("unset policy is empty")
        {
            const ExcludeNewerPolicy unset{};
            REQUIRE(unset.empty());
        }

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
