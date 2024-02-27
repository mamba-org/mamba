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

            static constexpr auto opens = std::array{ '[', '(' };
            static constexpr auto closes = std::array{ ']', ')' };
            CHECK_EQ(
                find_not_in_parentheses("(hello, world), [welcome, here],", ',', opens, closes),
                14
            );
            CHECK_EQ(
                find_not_in_parentheses("([(hello)], ([world])), [welcome, here],", ',', opens, closes),
                22
            );
            CHECK_EQ(find_not_in_parentheses("[hello, world](welcome, here),", ',', opens, closes), 29);
            CHECK_EQ(
                find_not_in_parentheses("(hello, ]world,) welcome, here],", ',', opens, closes).error(),
                ParseError::InvalidInput
            );

            // The following unfortunaltely does not work as we would need to allocate a stack
            // to keep track of the opening and closing of parentheses.
            // CHECK_EQ(
            //     find_not_in_parentheses("(hello, [world, )welcome, here],", ',', opens,
            //     closes).error(), ParseError::InvalidInput
            // );
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

            static constexpr auto opens = std::array{ '[', '(', '\'', '"' };
            static constexpr auto closes = std::array{ ']', ')', '\'', '"' };
            CHECK_EQ(
                find_not_in_parentheses(R"('("hello", world)', [welcome, here],)", ',', opens, closes),
                18
            );
            CHECK_EQ(
                find_not_in_parentheses("('[(hello)], ([world])'), [welcome, here],", ',', opens, closes),
                24
            );
            CHECK_EQ(
                find_not_in_parentheses("('hello', ']world,) welcome, here],", ',', opens, closes)
                    .error(),
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

            static constexpr auto opens = std::array{ '[', '(' };
            static constexpr auto closes = std::array{ ']', ')' };

            CHECK_EQ(
                find_not_in_parentheses("(some str)", "", opens, closes).error(),
                ParseError::InvalidInput
            );
            CHECK_EQ(
                find_not_in_parentheses(R"((hello , world), [welcome , here] ,elf)", " ,", opens, closes),
                33
            );
            CHECK_EQ(
                find_not_in_parentheses("([(hello)] , ([world])), [welcome , here] ,elf", " ,", opens, closes),
                41
            );
            CHECK_EQ(
                find_not_in_parentheses("(hello , ]world,) welcome, here],", ", ", opens, closes).error(),
                ParseError::InvalidInput
            );
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

            static constexpr auto opens = std::array{ '[', '(', '\'', '"' };
            static constexpr auto closes = std::array{ ']', ')', '\'', '"' };

            CHECK_EQ(
                find_not_in_parentheses("(some str)", "", opens, closes).error(),
                ParseError::InvalidInput
            );
            CHECK_EQ(
                find_not_in_parentheses(R"('("hello" , world)', [welcome , here] ,elf)", " ,", opens, closes),
                37
            );
            CHECK_EQ(
                find_not_in_parentheses(
                    "('[(hello)] , ([world])'), [welcome , here] ,elf",
                    " ,",
                    opens,
                    closes
                ),
                43
            );
            CHECK_EQ(
                find_not_in_parentheses("('hello' , ']world,) welcome, here],", ", ", opens, closes)
                    .error(),
                ParseError::InvalidInput
            );
        }
    }

    TEST_CASE("glob_match")
    {
        CHECK(glob_match("python", "python"));
        CHECK_FALSE(glob_match("cpython", "python"));
        CHECK_FALSE(glob_match("python", "cpython"));
        CHECK_FALSE(glob_match("python", ""));

        CHECK(glob_match("py*", "py"));
        CHECK(glob_match("py*", "py"));
        CHECK(glob_match("py*", "python"));
        CHECK_FALSE(glob_match("py*", "cpython"));
        CHECK_FALSE(glob_match("py*", ""));

        CHECK(glob_match("*37", "python37"));
        CHECK(glob_match("*37", "37"));
        CHECK_FALSE(glob_match("*37", "python37-linux64"));
        CHECK_FALSE(glob_match("*37", ""));

        CHECK(glob_match("*py*", "python"));
        CHECK(glob_match("*py*", "cpython"));
        CHECK(glob_match("*py*", "cpy"));
        CHECK_FALSE(glob_match("*py*", "linux"));
        CHECK_FALSE(glob_match("*py*", ""));

        CHECK(glob_match("*py*-3*-*-64", "cpython-37-linux-64"));
        CHECK(glob_match("*py*-3*-*-64", "python-37-more-linux-64"));
        CHECK_FALSE(glob_match("*py*-3*-*-64", "cpython-37-linux-64-more"));
        CHECK_FALSE(glob_match("*py*-3*-*-64", ""));

        CHECK(glob_match("py**", "python"));
        CHECK_FALSE(glob_match("py**", "cpython"));
        CHECK(glob_match("**37", "python37"));
        CHECK_FALSE(glob_match("**37", "python37-linux64"));
        CHECK(glob_match("**py**", "python"));
        CHECK_FALSE(glob_match("**py**", "linux"));
    }
}
