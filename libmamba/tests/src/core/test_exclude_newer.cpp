// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/error_handling.hpp"
#include "mamba/core/exclude_newer.hpp"

using namespace mamba;

namespace
{
    TEST_CASE("resolve_exclude_newer_cutoff")
    {
        constexpr std::uint64_t now = 1'700'000'000;

        SECTION("empty values disable the policy")
        {
            REQUIRE(resolve_exclude_newer_cutoff("", now) == std::nullopt);
        }

        SECTION("whitespace-only values cannot be parsed")
        {
            REQUIRE_THROWS_AS(resolve_exclude_newer_cutoff("   ", now), mamba_error);
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
            REQUIRE(resolve_exclude_newer_cutoff("1y", now) == now - 365 * 86400);
            REQUIRE(resolve_exclude_newer_cutoff("6M", now) == now - 6 * 30 * 86400);
            REQUIRE(
                resolve_exclude_newer_cutoff("1y6M7d", now)
                == now - (365 * 86400 + 6 * 30 * 86400 + 7 * 86400)
            );
        }

        SECTION("ISO 8601 durations resolve relative to now")
        {
            REQUIRE(resolve_exclude_newer_cutoff("P7D", now) == now - 7 * 86400);
            REQUIRE(resolve_exclude_newer_cutoff("PT24H", now) == now - 24 * 3600);
            REQUIRE(resolve_exclude_newer_cutoff("P1DT12H", now) == now - (86400 + 12 * 3600));
            REQUIRE(resolve_exclude_newer_cutoff("P1Y", now) == now - 365 * 86400);
            REQUIRE(resolve_exclude_newer_cutoff("P6M", now) == now - 6 * 30 * 86400);
            REQUIRE(resolve_exclude_newer_cutoff("PT1M", now) == now - 60);
            REQUIRE(
                resolve_exclude_newer_cutoff("P3Y6M4DT12H30M5S", now)
                == now - (3 * 365 * 86400 + 6 * 30 * 86400 + 4 * 86400 + 12 * 3600 + 30 * 60 + 5)
            );
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
            REQUIRE_THROWS_AS(resolve_exclude_newer_cutoff("not-a-duration", now), mamba_error);
            REQUIRE_THROWS_AS(resolve_exclude_newer_cutoff("P", now), mamba_error);
        }
    }

    TEST_CASE("parse_chrono")
    {
        using SysDays = std::chrono::sys_days;
        using SysTime = std::chrono::sys_time<std::chrono::seconds>;

        const auto day_epoch = [](std::string_view value) -> std::optional<std::int64_t>
        {
            const auto parsed = detail::parse_chrono<SysDays>(value, "%F");
            if (!parsed)
            {
                return std::nullopt;
            }
            return parsed->time_since_epoch().count();
        };

        const auto time_epoch = [](std::string_view value,
                                   const char* fmt) -> std::optional<std::int64_t>
        {
            const auto parsed = detail::parse_chrono<SysTime>(value, fmt);
            if (!parsed)
            {
                return std::nullopt;
            }
            return std::chrono::duration_cast<std::chrono::seconds>(parsed->time_since_epoch()).count();
        };

        SECTION("date-only values parse with %F")
        {
            REQUIRE(day_epoch("2026-04-01") == 20'544);
            REQUIRE(day_epoch("2026-01-31") == 20'484);
            REQUIRE(day_epoch("2024-02-29") == 19'782);
        }

        SECTION("RFC 3339 datetimes parse to UTC instants")
        {
            REQUIRE(time_epoch("2026-04-01T12:00:00", "%FT%T") == 1'775'044'800);
            REQUIRE(time_epoch("2026-04-01T10:00:00Z", "%FT%TZ") == 1'775'037'600);
            REQUIRE(time_epoch("2026-04-01T12:00:00+02:00", "%FT%T%Ez") == 1'775'037'600);
        }

        SECTION("invalid values are rejected")
        {
            REQUIRE(day_epoch("") == std::nullopt);
            REQUIRE(day_epoch("2026/04/01") == std::nullopt);
            REQUIRE(day_epoch("not-a-date") == std::nullopt);
            REQUIRE(day_epoch("2026-04-01T12:00:00") == std::nullopt);
            REQUIRE(day_epoch("2026-04-01 extra") == std::nullopt);

            REQUIRE(time_epoch("", "%FT%T") == std::nullopt);
            REQUIRE(time_epoch("2026-04-01", "%FT%T") == std::nullopt);
            REQUIRE(time_epoch("2026-04-01T12:00:00", "%FT%TZ") == std::nullopt);
            REQUIRE(time_epoch("2026-04-01T12:00:00extra", "%FT%T") == std::nullopt);
            REQUIRE(time_epoch("2026-04-01T12:00:00+0200", "%FT%T%Ez") == std::nullopt);
        }
    }

    TEST_CASE("parse_iso8601_duration_seconds")
    {
        SECTION("malformed durations are rejected")
        {
            REQUIRE(detail::parse_iso8601_duration_seconds("P3Y6M4D12H30M5S") == std::nullopt);
            REQUIRE(detail::parse_iso8601_duration_seconds("3Y6M4DT12H30M5S") == std::nullopt);
            REQUIRE(detail::parse_iso8601_duration_seconds("12H30M5S") == std::nullopt);
        }
    }

    TEST_CASE("parse_compact_duration_seconds")
    {
        constexpr std::uint64_t y = 365 * 86400;
        constexpr std::uint64_t mon = 30 * 86400;
        constexpr std::uint64_t w = 604800;
        constexpr std::uint64_t d = 86400;
        constexpr std::uint64_t h = 3600;
        constexpr std::uint64_t min = 60;

        const auto sec = [](std::int64_t n) { return std::chrono::seconds{ n }; };
        const auto parse = [](std::string_view value)
        { return detail::parse_compact_duration_seconds(value); };

        SECTION("each unit suffix is parsed on its own")
        {
            REQUIRE(parse("1y") == sec(static_cast<std::int64_t>(y)));
            REQUIRE(parse("2M") == sec(static_cast<std::int64_t>(2 * mon)));
            REQUIRE(parse("3w") == sec(static_cast<std::int64_t>(3 * w)));
            REQUIRE(parse("4d") == sec(static_cast<std::int64_t>(4 * d)));
            REQUIRE(parse("5h") == sec(static_cast<std::int64_t>(5 * h)));
            REQUIRE(parse("6m") == sec(static_cast<std::int64_t>(6 * min)));
            REQUIRE(parse("7s") == sec(7));
        }

        SECTION("zero amounts are valid")
        {
            REQUIRE(parse("0y") == sec(0));
            REQUIRE(parse("0M") == sec(0));
            REQUIRE(parse("0w") == sec(0));
            REQUIRE(parse("0d") == sec(0));
            REQUIRE(parse("0h") == sec(0));
            REQUIRE(parse("0m") == sec(0));
            REQUIRE(parse("0s") == sec(0));
            REQUIRE(parse("0y0M0w0d0h0m0s") == sec(0));
        }

        SECTION("multiple segments are summed")
        {
            REQUIRE(parse("3d12h") == sec(static_cast<std::int64_t>(3 * d + 12 * h)));
            REQUIRE(parse("1w2d") == sec(static_cast<std::int64_t>(w + 2 * d)));
            REQUIRE(parse("1y6M7d") == sec(static_cast<std::int64_t>(y + 6 * mon + 7 * d)));
            REQUIRE(
                parse("1y2M3w4d5h6m7s")
                == sec(static_cast<std::int64_t>(y + 2 * mon + 3 * w + 4 * d + 5 * h + 6 * min + 7))
            );
        }

        SECTION("minutes and months are distinguished by case")
        {
            REQUIRE(parse("30m") == sec(30 * min));
            REQUIRE(parse("30M") == sec(static_cast<std::int64_t>(30 * mon)));
            REQUIRE(parse("1m2M") == sec(static_cast<std::int64_t>(min + 2 * mon)));
        }

        SECTION("unit letters are case-sensitive")
        {
            REQUIRE(parse("7D") == std::nullopt);
            REQUIRE(parse("1Y") == std::nullopt);
            REQUIRE(parse("1W") == std::nullopt);
            REQUIRE(parse("12H") == std::nullopt);
            REQUIRE(parse("30S") == std::nullopt);
            REQUIRE(parse("6m") == sec(6 * min));
            REQUIRE(parse("6M") == sec(static_cast<std::int64_t>(6 * mon)));
        }

        SECTION("incomplete or malformed compact durations are rejected")
        {
            REQUIRE(parse("") == std::nullopt);
            REQUIRE(parse("7") == std::nullopt);
            REQUIRE(parse("7 ") == std::nullopt);
            REQUIRE(parse("d7") == std::nullopt);
            REQUIRE(parse("7d12") == std::nullopt);
            REQUIRE(parse("7d12x") == std::nullopt);
            REQUIRE(parse("-1d") == std::nullopt);
            REQUIRE(parse("1.5d") == std::nullopt);
            REQUIRE(parse("1x") == std::nullopt);
            REQUIRE(parse("not-a-duration") == std::nullopt);
        }

        SECTION("values handled by other parsers are not compact durations")
        {
            REQUIRE(parse("3600") == std::nullopt);
            REQUIRE(parse("0") == std::nullopt);
            REQUIRE(parse("P7D") == std::nullopt);
            REQUIRE(parse("PT1H") == std::nullopt);
            REQUIRE(parse("2026-04-01") == std::nullopt);
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
