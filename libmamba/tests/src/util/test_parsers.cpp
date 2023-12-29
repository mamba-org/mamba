// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <doctest/doctest.h>

#include "mamba/util/parsers.hpp"

using namespace mamba::util;

TEST_SUITE("util::parsers")
{
    TEST_CASE("find_matching_parentheses")
    {
        SUBCASE("Different open/close pair")
        {
            CHECK_EQ(find_matching_parentheses_str("").error(), ParseError::NotFound);
            CHECK_EQ(find_matching_parentheses_str("Nothing to see here").error(), ParseError::NotFound);
            CHECK_EQ(find_matching_parentheses_str("(hello)", '[', ']').error(), ParseError::NotFound);

            CHECK_EQ(find_matching_parentheses_str("()"), "()");
            CHECK_EQ(find_matching_parentheses_str("hello()"), "()");
            CHECK_EQ(find_matching_parentheses_str("(hello)"), "(hello)");
            CHECK_EQ(
                find_matching_parentheses_str("(hello (dear (sir))(!))(how(are(you)))"),
                "(hello (dear (sir))(!))"
            );
            CHECK_EQ(find_matching_parentheses_str("[hello]", '[', ']'), "[hello]");

            CHECK_EQ(find_matching_parentheses_str(")(").error(), ParseError::InvalidInput);
            CHECK_EQ(find_matching_parentheses_str("((hello)").error(), ParseError::InvalidInput);
        }

        SUBCASE("Similar open/close pair")
        {
            CHECK_EQ(find_matching_parentheses_str(R"("")", '"', '"'), R"("")");
            CHECK_EQ(find_matching_parentheses_str(R"("hello")", '"', '"'), R"("hello")");
            CHECK_EQ(find_matching_parentheses_str(R"("some","csv","value")", '"', '"'), R"("some")");
            CHECK_EQ(
                find_matching_parentheses_str(R"(Here is "some)", '"', '"').error(),
                ParseError::InvalidInput
            );
        }
    }

    TEST_CASE("find_not_in_parentheses")
    {
        SUBCASE("Single char and different open/close pair")
        {
            CHECK_EQ(find_not_in_parentheses("", ',').error(), ParseError::NotFound);
            CHECK_EQ(find_not_in_parentheses("Nothing to see here", ',').error(), ParseError::NotFound);
            CHECK_EQ(find_not_in_parentheses("(hello, world)", ',').error(), ParseError::NotFound);

            CHECK_EQ(find_not_in_parentheses("hello, world", ','), 5);
            CHECK_EQ(find_not_in_parentheses("hello, world, welcome", ','), 5);
            CHECK_EQ(find_not_in_parentheses("(hello, world), (welcome, here),", ','), 14);
            CHECK_EQ(find_not_in_parentheses("(hello, world), (welcome, here),", ',', '[', ']'), 6);
            CHECK_EQ(find_not_in_parentheses("[hello, world](welcome, here),", ',', '[', ']'), 22);

            CHECK_EQ(find_not_in_parentheses("(hello, world,", ',').error(), ParseError::InvalidInput);
            CHECK_EQ(find_not_in_parentheses("(hello", ',').error(), ParseError::InvalidInput);
        }

        SUBCASE("Single char and similar open/close pair")
        {
            CHECK_EQ(
                find_not_in_parentheses(R"("some, csv")", ',', '"', '"').error(),
                ParseError::NotFound
            );

            CHECK_EQ(find_not_in_parentheses(R"("some, csv",value)", ',', '"', '"'), 11);
            CHECK_EQ(find_not_in_parentheses(R"("some, csv""value","here")", ',', '"', '"'), 18);

            CHECK_EQ(
                find_not_in_parentheses(R"("some, csv)", ',', '"', '"').error(),
                ParseError::InvalidInput
            );
        }

        SUBCASE("Substring and different open/close pair")
        {
            CHECK_EQ(find_not_in_parentheses("", "::").error(), ParseError::NotFound);
            CHECK_EQ(find_not_in_parentheses("Nothing to see here", "::").error(), ParseError::NotFound);
            CHECK_EQ(find_not_in_parentheses("(hello::world)", "::").error(), ParseError::NotFound);

            CHECK_EQ(find_not_in_parentheses("hello::world", "::"), 5);
            CHECK_EQ(find_not_in_parentheses("hello::world::welcome", "::"), 5);
            CHECK_EQ(find_not_in_parentheses("(hello::world)::(welcome::here)::", "::").value(), 14);
            CHECK_EQ(find_not_in_parentheses("(hello::world)::(welcome::here)", "::", '[', ']'), 6);
            CHECK_EQ(find_not_in_parentheses("[hello::world](welcome::here),", "::", '[', ']'), 22);
            //
            CHECK_EQ(find_not_in_parentheses("(hello::world::", "::").error(), ParseError::InvalidInput);
            CHECK_EQ(find_not_in_parentheses("(hello", "::").error(), ParseError::InvalidInput);
        }

        SUBCASE("Substring and similar open/close pair")
        {
            CHECK_EQ(
                find_not_in_parentheses(R"("some::csv")", "::", '"', '"').error(),
                ParseError::NotFound
            );

            CHECK_EQ(find_not_in_parentheses(R"("some::csv"::value)", "::", '"', '"'), 11);
            CHECK_EQ(find_not_in_parentheses(R"("some::csv""value"::"here")", "::", '"', '"'), 18);

            CHECK_EQ(
                find_not_in_parentheses(R"("some::csv)", "::", '"', '"').error(),
                ParseError::InvalidInput
            );
        }
    }
}
