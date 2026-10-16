// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <chrono>
#include <optional>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "mamba/core/detail/chrono_parse.hpp"

using namespace mamba;

namespace
{
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
}
