// Copyright (c) 2026, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/core/error_handling.hpp"

#include "core/link.hpp"

namespace mamba
{
    namespace
    {
        TEST_CASE("parse_entry_point accepts valid definitions")
        {
            // Spaces around '=' are stripped; the command name itself must not contain spaces.
            const auto parsed = parse_entry_point("pip = pip._internal.cli.main:main");
            REQUIRE(parsed.has_value());
            REQUIRE(parsed->command == "pip");
            REQUIRE(parsed->module == "pip._internal.cli.main");
            REQUIRE(parsed->func == "main");
        }

        TEST_CASE("parse_entry_point accepts quoted module:callable (findpython / conda#16339)")
        {
            // Some noarch packages ship entry points with quotes around the RHS, e.g.
            // findpython-0.7.1-pyh332efcf_0's info/link.json.
            const auto parsed = parse_entry_point(R"(findpython = "findpython.__main__:main")");
            REQUIRE(parsed.has_value());
            REQUIRE(parsed->command == "findpython");
            REQUIRE(parsed->module == "findpython.__main__");
            REQUIRE(parsed->func == "main");

            const auto parsed_sq = parse_entry_point("cmd = 'pkg.mod:func'");
            REQUIRE(parsed_sq.has_value());
            REQUIRE(parsed_sq->command == "cmd");
            REQUIRE(parsed_sq->module == "pkg.mod");
            REQUIRE(parsed_sq->func == "func");
        }

        TEST_CASE("parse_entry_point multiple validation errors stay invalid_spec")
        {
            // Must not surface as error_code::aggregated via a sliced mamba_aggregated_error
            // (that made micromamba main segfault on static_cast; mamba-org/mamba#4352).
            const auto parsed = parse_entry_point(R"(bad/cmd = "..evil:also/bad")");
            REQUIRE_FALSE(parsed.has_value());
            REQUIRE(parsed.error().error_code() == mamba_error_code::invalid_spec);
        }

        TEST_CASE("parse_entry_point rejects path traversal in command")
        {
            REQUIRE_FALSE(parse_entry_point("../../../tmp/PWN = innocuous_pkg.evil:main").has_value());
            REQUIRE_FALSE(parse_entry_point("../bin/pip = innocuous_pkg.evil:main").has_value());
            REQUIRE_FALSE(parse_entry_point("/etc/cron.d/zzz = innocuous_pkg.evil:main").has_value());
        }

        TEST_CASE("parse_entry_point rejects Windows drive paths on all platforms")
        {
            REQUIRE_FALSE(parse_entry_point("C:\\evil = innocuous_pkg.evil:main").has_value());
            REQUIRE_FALSE(parse_entry_point("C:evil = innocuous_pkg.evil:main").has_value());
        }

        TEST_CASE("parse_entry_point rejects whitespace in command name")
        {
            REQUIRE_FALSE(parse_entry_point("my cmd = pkg.mod:main").has_value());
        }

        TEST_CASE("parse_entry_point rejects invalid module or callable")
        {
            REQUIRE_FALSE(parse_entry_point("pip = ..evil:main").has_value());
            REQUIRE_FALSE(parse_entry_point("pip = pip.evil:../../main").has_value());
        }

        TEST_CASE("parse_entry_point rejects malformed definitions")
        {
            REQUIRE_FALSE(parse_entry_point("not-an-entry-point").has_value());
            REQUIRE_FALSE(parse_entry_point("no-colon = pkg.mod").has_value());
        }

        TEST_CASE("parse_entry_point returns mamba_error")
        {
            const auto parsed = parse_entry_point("../bin/pip = innocuous_pkg.evil:main");
            REQUIRE_FALSE(parsed.has_value());
            REQUIRE(parsed.error().error_code() == mamba_error_code::invalid_spec);
        }
    }
}  // namespace mamba
