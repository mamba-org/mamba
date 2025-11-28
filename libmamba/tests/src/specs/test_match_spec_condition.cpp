// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/specs/match_spec.hpp"
#include "mamba/specs/match_spec_condition.hpp"
#include "mamba/specs/package_info.hpp"

using namespace mamba;
using namespace mamba::specs;

TEST_CASE("MatchSpecCondition parse", "[mamba::specs][mamba::specs::MatchSpecCondition]")
{
    SECTION("Simple condition: python >=3.10")
    {
        auto cond = MatchSpecCondition::parse("python >=3.10");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "python>=3.10");
    }

    SECTION("Simple condition: __unix")
    {
        auto cond = MatchSpecCondition::parse("__unix");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "__unix");
    }

    SECTION("Simple condition: __win")
    {
        auto cond = MatchSpecCondition::parse("__win");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "__win");
    }

    SECTION("Simple condition: python <3.8")
    {
        auto cond = MatchSpecCondition::parse("python <3.8");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "python<3.8");
    }

    SECTION("OR condition: python >=3.10 or pypy")
    {
        auto cond = MatchSpecCondition::parse("python >=3.10 or pypy");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "(python>=3.10 or pypy)");
    }

    SECTION("AND condition: python >=3.10 and numpy >=2.0")
    {
        auto cond = MatchSpecCondition::parse("python >=3.10 and numpy >=2.0");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "(python>=3.10 and numpy>=2.0)");
    }

    SECTION("Complex OR: a or b or c")
    {
        auto cond = MatchSpecCondition::parse("a or b or c");
        REQUIRE(cond.has_value());
        // Should be left-associative: ((a or b) or c)
        REQUIRE(cond->to_string() == "((a or b) or c)");
    }

    SECTION("Complex AND: a and b and c")
    {
        auto cond = MatchSpecCondition::parse("a and b and c");
        REQUIRE(cond.has_value());
        // Should be left-associative: ((a and b) and c)
        REQUIRE(cond->to_string() == "((a and b) and c)");
    }

    SECTION("Precedence: a and b or c")
    {
        auto cond = MatchSpecCondition::parse("a and b or c");
        REQUIRE(cond.has_value());
        // AND has higher precedence: ((a and b) or c)
        REQUIRE(cond->to_string() == "((a and b) or c)");
    }

    SECTION("Precedence: a or b and c")
    {
        auto cond = MatchSpecCondition::parse("a or b and c");
        REQUIRE(cond.has_value());
        // AND has higher precedence: (a or (b and c))
        REQUIRE(cond->to_string() == "(a or (b and c))");
    }

    SECTION("Parentheses: (a or b) and c")
    {
        auto cond = MatchSpecCondition::parse("(a or b) and c");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "((a or b) and c)");
    }

    SECTION("Nested parentheses: (a or (b and c))")
    {
        auto cond = MatchSpecCondition::parse("(a or (b and c))");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "(a or (b and c))");
    }

    SECTION("Deep nesting: a and (b or (c and d))")
    {
        auto cond = MatchSpecCondition::parse("a and (b or (c and d))");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "(a and (b or (c and d)))");
    }

    SECTION("Whitespace handling")
    {
        auto cond = MatchSpecCondition::parse("  python  >=  3.10  ");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "python>=3.10");
    }

    SECTION("Case insensitive operators: AND")
    {
        auto cond = MatchSpecCondition::parse("a AND b");
        if (!cond.has_value())
        {
            INFO("Parse error: " << cond.error().what());
        }
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "(a and b)");
    }

    SECTION("Case insensitive operators: OR")
    {
        auto cond = MatchSpecCondition::parse("a OR b");
        if (!cond.has_value())
        {
            INFO("Parse error: " << cond.error().what());
        }
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "(a or b)");
    }

    SECTION("Real-world example: python <3.8 or pypy")
    {
        auto cond = MatchSpecCondition::parse("python <3.8 or pypy");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "(python<3.8 or pypy)");
    }

    SECTION("Real-world example: python >=3.9 and numpy >=2.0")
    {
        auto cond = MatchSpecCondition::parse("python >=3.9 and numpy >=2.0");
        REQUIRE(cond.has_value());
        REQUIRE(cond->to_string() == "(python>=3.9 and numpy>=2.0)");
    }

    SECTION("Error: empty string")
    {
        auto cond = MatchSpecCondition::parse("");
        REQUIRE_FALSE(cond.has_value());
    }

    SECTION("Error: unmatched opening parenthesis")
    {
        auto cond = MatchSpecCondition::parse("(a or b");
        REQUIRE_FALSE(cond.has_value());
    }

    SECTION("Error: unmatched closing parenthesis")
    {
        auto cond = MatchSpecCondition::parse("a or b)");
        REQUIRE_FALSE(cond.has_value());
    }

    SECTION("Error: and without operand")
    {
        auto cond = MatchSpecCondition::parse("a and");
        REQUIRE_FALSE(cond.has_value());
    }

    SECTION("Error: or without operand")
    {
        auto cond = MatchSpecCondition::parse("a or");
        REQUIRE_FALSE(cond.has_value());
    }

    SECTION("Error: invalid matchspec")
    {
        auto cond = MatchSpecCondition::parse("invalid matchspec with spaces");
        REQUIRE_FALSE(cond.has_value());
    }
}

TEST_CASE("MatchSpecCondition equality", "[mamba::specs][mamba::specs::MatchSpecCondition]")
{
    SECTION("Simple conditions")
    {
        auto cond1 = MatchSpecCondition::parse("python >=3.10").value();
        auto cond2 = MatchSpecCondition::parse("python >=3.10").value();
        REQUIRE(cond1 == cond2);
    }

    SECTION("Different conditions")
    {
        auto cond1 = MatchSpecCondition::parse("python >=3.10").value();
        auto cond2 = MatchSpecCondition::parse("python <3.10").value();
        REQUIRE_FALSE(cond1 == cond2);
    }

    SECTION("OR conditions")
    {
        auto cond1 = MatchSpecCondition::parse("a or b").value();
        auto cond2 = MatchSpecCondition::parse("a or b").value();
        REQUIRE(cond1 == cond2);
    }

    SECTION("AND conditions")
    {
        auto cond1 = MatchSpecCondition::parse("a and b").value();
        auto cond2 = MatchSpecCondition::parse("a and b").value();
        REQUIRE(cond1 == cond2);
    }

    SECTION("Complex conditions")
    {
        auto cond1 = MatchSpecCondition::parse("(a or b) and c").value();
        auto cond2 = MatchSpecCondition::parse("(a or b) and c").value();
        REQUIRE(cond1 == cond2);
    }
}

TEST_CASE("MatchSpecCondition contains", "[mamba::specs][mamba::specs::MatchSpecCondition]")
{
    SECTION("Simple condition: python")
    {
        auto cond = MatchSpecCondition::parse("python").value();

        auto python_pkg = PackageInfo("python", "3.10.0", "", 0);
        REQUIRE(cond.contains(python_pkg));

        auto numpy_pkg = PackageInfo("numpy", "1.21.0", "", 0);
        REQUIRE_FALSE(cond.contains(numpy_pkg));
    }

    SECTION("Simple condition: python >=3.10")
    {
        auto cond = MatchSpecCondition::parse("python >=3.10").value();

        auto python310 = PackageInfo("python", "3.10.0", "", 0);
        REQUIRE(cond.contains(python310));

        auto python39 = PackageInfo("python", "3.9.0", "", 0);
        REQUIRE_FALSE(cond.contains(python39));
    }

    SECTION("Simple condition: python <3.10")
    {
        auto cond = MatchSpecCondition::parse("python <3.10").value();

        auto python39 = PackageInfo("python", "3.9.0", "", 0);
        REQUIRE(cond.contains(python39));

        auto python310 = PackageInfo("python", "3.10.0", "", 0);
        REQUIRE_FALSE(cond.contains(python310));

        auto python311 = PackageInfo("python", "3.11.0", "", 0);
        REQUIRE_FALSE(cond.contains(python311));
    }

    SECTION("OR condition")
    {
        auto cond = MatchSpecCondition::parse("python or numpy").value();

        auto python_pkg = PackageInfo("python", "3.10.0", "", 0);
        REQUIRE(cond.contains(python_pkg));

        auto numpy_pkg = PackageInfo("numpy", "1.21.0", "", 0);
        REQUIRE(cond.contains(numpy_pkg));

        auto scipy_pkg = PackageInfo("scipy", "1.7.0", "", 0);
        REQUIRE_FALSE(cond.contains(scipy_pkg));
    }

    SECTION("AND condition")
    {
        auto cond = MatchSpecCondition::parse("python >=3.10 and numpy").value();

        auto python310 = PackageInfo("python", "3.10.0", "", 0);
        // Note: contains() checks individual packages, not combinations
        // This is a limitation - full evaluation needs solver context
        // For AND conditions, both must match, so this will fail
        REQUIRE_FALSE(cond.contains(python310));
    }
}

TEST_CASE("MatchSpecCondition hash", "[mamba::specs][mamba::specs::MatchSpecCondition]")
{
    SECTION("Same conditions hash to same value")
    {
        auto cond1 = MatchSpecCondition::parse("python >=3.10").value();
        auto cond2 = MatchSpecCondition::parse("python >=3.10").value();

        auto hash1 = std::hash<MatchSpecCondition>{}(cond1);
        auto hash2 = std::hash<MatchSpecCondition>{}(cond2);
        REQUIRE(hash1 == hash2);
    }

    SECTION("Different conditions hash to different values")
    {
        auto cond1 = MatchSpecCondition::parse("python >=3.10").value();
        auto cond2 = MatchSpecCondition::parse("python <3.10").value();

        auto hash1 = std::hash<MatchSpecCondition>{}(cond1);
        auto hash2 = std::hash<MatchSpecCondition>{}(cond2);
        REQUIRE(hash1 != hash2);
    }

    SECTION("OR conditions hash correctly")
    {
        auto cond1 = MatchSpecCondition::parse("python >=3.10 or pypy").value();
        auto cond2 = MatchSpecCondition::parse("python >=3.10 or pypy").value();
        auto cond3 = MatchSpecCondition::parse("python >=3.10 and pypy").value();

        // Same OR conditions should hash to same value
        auto hash1 = std::hash<MatchSpecCondition>{}(cond1);
        auto hash2 = std::hash<MatchSpecCondition>{}(cond2);
        REQUIRE(hash1 == hash2);

        // Different logical operators should hash to different values
        auto hash3 = std::hash<MatchSpecCondition>{}(cond3);
        REQUIRE(hash1 != hash3);
    }

    SECTION("AND conditions hash correctly")
    {
        auto cond1 = MatchSpecCondition::parse("python >=3.10 and numpy").value();
        auto cond2 = MatchSpecCondition::parse("python >=3.10 and numpy").value();
        auto cond3 = MatchSpecCondition::parse("python >=3.10 or numpy").value();

        // Same AND conditions should hash to same value
        auto hash1 = std::hash<MatchSpecCondition>{}(cond1);
        auto hash2 = std::hash<MatchSpecCondition>{}(cond2);
        REQUIRE(hash1 == hash2);

        // Different logical operators should hash to different values
        auto hash3 = std::hash<MatchSpecCondition>{}(cond3);
        REQUIRE(hash1 != hash3);
    }

    SECTION("Nested conditions hash correctly")
    {
        auto cond1 = MatchSpecCondition::parse("(python >=3.10 or pypy) and numpy").value();
        auto cond2 = MatchSpecCondition::parse("(python >=3.10 or pypy) and numpy").value();
        auto cond3 = MatchSpecCondition::parse("(python >=3.10 or pypy)").value();

        // Same nested conditions should hash to same value
        auto hash1 = std::hash<MatchSpecCondition>{}(cond1);
        auto hash2 = std::hash<MatchSpecCondition>{}(cond2);
        REQUIRE(hash1 == hash2);

        // Different nested structures should hash to different values
        auto hash3 = std::hash<MatchSpecCondition>{}(cond3);
        REQUIRE(hash1 != hash3);
    }

    SECTION("Hash consistency: same condition hashed multiple times")
    {
        auto cond = MatchSpecCondition::parse("python >=3.10").value();

        // Hash multiple times - should be consistent
        auto hash1 = std::hash<MatchSpecCondition>{}(cond);
        auto hash2 = std::hash<MatchSpecCondition>{}(cond);
        auto hash3 = std::hash<MatchSpecCondition>{}(cond);

        REQUIRE(hash1 == hash2);
        REQUIRE(hash2 == hash3);
    }

    SECTION("Platform conditions hash correctly")
    {
        auto cond1 = MatchSpecCondition::parse("__unix").value();
        auto cond2 = MatchSpecCondition::parse("__unix").value();
        auto cond3 = MatchSpecCondition::parse("__win").value();

        // Same platform conditions should hash to same value
        auto hash1 = std::hash<MatchSpecCondition>{}(cond1);
        auto hash2 = std::hash<MatchSpecCondition>{}(cond2);
        REQUIRE(hash1 == hash2);

        // Different platform conditions should hash to different values
        auto hash3 = std::hash<MatchSpecCondition>{}(cond3);
        REQUIRE(hash1 != hash3);
    }
}
