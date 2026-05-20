// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "core/link.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("parse_entry_point accepts valid definitions")
        {
            const auto parsed = parse_entry_point("pip = pip._internal.cli.main:main");
            REQUIRE(parsed.command == "pip");
            REQUIRE(parsed.module == "pip._internal.cli.main");
            REQUIRE(parsed.func == "main");
        }

        TEST_CASE("parse_entry_point rejects path traversal in command")
        {
            REQUIRE_THROWS_AS(
                parse_entry_point("../../../tmp/PWN = innocuous_pkg.evil:main"),
                std::runtime_error
            );
            REQUIRE_THROWS_AS(
                parse_entry_point("../bin/pip = innocuous_pkg.evil:main"),
                std::runtime_error
            );
            REQUIRE_THROWS_AS(
                parse_entry_point("/etc/cron.d/zzz = innocuous_pkg.evil:main"),
                std::runtime_error
            );
        }

        TEST_CASE("parse_entry_point rejects invalid module or callable")
        {
            REQUIRE_THROWS_AS(parse_entry_point("pip = ..evil:main"), std::runtime_error);
            REQUIRE_THROWS_AS(parse_entry_point("pip = pip.evil:../../main"), std::runtime_error);
        }

        TEST_CASE("parse_entry_point rejects malformed definitions")
        {
            REQUIRE_THROWS_AS(parse_entry_point("not-an-entry-point"), std::runtime_error);
            REQUIRE_THROWS_AS(parse_entry_point("no-colon = pkg.mod"), std::runtime_error);
        }
    }
}  // namespace mamba
