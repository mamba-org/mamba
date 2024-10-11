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
    inline static constexpr auto npos = std::string_view::npos;

    TEST_CASE("find_matching_parentheses")
    {
        using Slice = std::pair<std::size_t, std::size_t>;

        SUBCASE("Different open/close pair")
        {
            CHECK_EQ(find_matching_parentheses(""), Slice(npos, npos));
            CHECK_EQ(find_matching_parentheses("Nothing to see here"), Slice(npos, npos));
            CHECK_EQ(find_matching_parentheses("(hello)", '[', ']'), Slice(npos, npos));

            CHECK_EQ(find_matching_parentheses("()"), Slice(0, 1));
            CHECK_EQ(find_matching_parentheses("hello()"), Slice(5, 6));
            CHECK_EQ(find_matching_parentheses("(hello)"), Slice(0, 6));
            CHECK_EQ(find_matching_parentheses("(hello (dear (sir))(!))(how(are(you)))"), Slice(0, 22));
            CHECK_EQ(find_matching_parentheses("[hello]", '[', ']'), Slice(0, 6));

            CHECK_EQ(find_matching_parentheses(")(").error(), ParseError::InvalidInput);
            CHECK_EQ(find_matching_parentheses("((hello)").error(), ParseError::InvalidInput);

            static constexpr auto opens = std::array{ '[', '(' };
            static constexpr auto closes = std::array{ ']', ')' };
            CHECK_EQ(find_matching_parentheses("([hello])", opens, closes), Slice(0, 8));
            CHECK_EQ(find_matching_parentheses("(hello)[hello]", opens, closes), Slice(0, 6));
        }

        SUBCASE("Similar open/close pair")
        {
            CHECK_EQ(find_matching_parentheses(R"("")", '"', '"'), Slice(0, 1));
            CHECK_EQ(find_matching_parentheses(R"("hello")", '"', '"'), Slice(0, 6));
            CHECK_EQ(find_matching_parentheses(R"("some","csv","value")", '"', '"'), Slice(0, 5));
            CHECK_EQ(
                find_matching_parentheses(R"(Here is "some)", '"', '"').error(),
                ParseError::InvalidInput
            );

            static constexpr auto opens = std::array{ '[', '(', '\'' };
            static constexpr auto closes = std::array{ ']', ')', '\'' };
            CHECK_EQ(find_matching_parentheses("'[hello]'", opens, closes), Slice(0, 8));
            CHECK_EQ(find_matching_parentheses("hello['hello', 'world']", opens, closes), Slice(5, 22));
        }
    }

    TEST_CASE("rfind_matching_parentheses")
    {
        using Slice = std::pair<std::size_t, std::size_t>;

        SUBCASE("Different open/close pair")
        {
            CHECK_EQ(rfind_matching_parentheses(""), Slice(npos, npos));
            CHECK_EQ(rfind_matching_parentheses("Nothing to see here"), Slice(npos, npos));
            CHECK_EQ(rfind_matching_parentheses("(hello)", '[', ']'), Slice(npos, npos));

            CHECK_EQ(rfind_matching_parentheses("()"), Slice(0, 1));
            CHECK_EQ(rfind_matching_parentheses("hello()"), Slice(5, 6));
            CHECK_EQ(rfind_matching_parentheses("(hello)dear"), Slice(0, 6));
            CHECK_EQ(
                rfind_matching_parentheses("(hello (dear (sir))(!))(how(are(you)))"),
                Slice(23, 37)
            );
            CHECK_EQ(rfind_matching_parentheses("[hello]", '[', ']'), Slice(0, 6));

            CHECK_EQ(rfind_matching_parentheses(")(").error(), ParseError::InvalidInput);
            CHECK_EQ(rfind_matching_parentheses("(hello))").error(), ParseError::InvalidInput);

            static constexpr auto opens = std::array{ '[', '(' };
            static constexpr auto closes = std::array{ ']', ')' };
            CHECK_EQ(rfind_matching_parentheses("([hello])", opens, closes), Slice(0, 8));
            CHECK_EQ(rfind_matching_parentheses("(hello)[hello]", opens, closes), Slice(7, 13));
        }

        SUBCASE("Similar open/close pair")
        {
            CHECK_EQ(rfind_matching_parentheses(R"("")", '"', '"'), Slice(0, 1));
            CHECK_EQ(rfind_matching_parentheses(R"("hello")", '"', '"'), Slice(0, 6));
            CHECK_EQ(rfind_matching_parentheses(R"("some","csv","value")", '"', '"'), Slice(13, 19));
            CHECK_EQ(
                rfind_matching_parentheses(R"(Here is "some)", '"', '"').error(),
                ParseError::InvalidInput
            );

            static constexpr auto opens = std::array{ '[', '(', '\'' };
            static constexpr auto closes = std::array{ ']', ')', '\'' };
            CHECK_EQ(rfind_matching_parentheses("'[hello]'", opens, closes), Slice(0, 8));
            CHECK_EQ(rfind_matching_parentheses("['hello', 'world']dear", opens, closes), Slice(0, 17));
        }
    }

    TEST_CASE("find_not_in_parentheses")
    {
        SUBCASE("Single char and different open/close pair")
        {
            CHECK_EQ(find_not_in_parentheses("", ','), npos);
            CHECK_EQ(find_not_in_parentheses("Nothing to see here", ','), npos);
            CHECK_EQ(find_not_in_parentheses("(hello, world)", ','), npos);

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

            // The following unfortunately does not work as we would need to allocate a stack
            // to keep track of the opening and closing of parentheses.
            // CHECK_EQ(
            //     find_not_in_parentheses("(hello, [world, )welcome, here],", ',', opens,
            //     closes).error(), ParseError::InvalidInput
            // );
        }

        SUBCASE("Single char and similar open/close pair")
        {
            CHECK_EQ(find_not_in_parentheses(R"("some, csv")", ',', '"', '"'), npos);

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
            CHECK_EQ(find_not_in_parentheses("", "::"), npos);
            CHECK_EQ(find_not_in_parentheses("Nothing to see here", "::"), npos);
            CHECK_EQ(find_not_in_parentheses("(hello::world)", "::"), npos);

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
            CHECK_EQ(find_not_in_parentheses(R"("some::csv")", "::", '"', '"'), npos);

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

    TEST_CASE("rfind_not_in_parentheses")
    {
        SUBCASE("Single char and different open/close pair")
        {
            CHECK_EQ(rfind_not_in_parentheses("", ','), npos);
            CHECK_EQ(rfind_not_in_parentheses("Nothing to see here", ','), npos);
            CHECK_EQ(rfind_not_in_parentheses("(hello, world)", ','), npos);

            CHECK_EQ(rfind_not_in_parentheses("hello, world", ','), 5);
            CHECK_EQ(rfind_not_in_parentheses("hello, world, welcome", ','), 12);
            CHECK_EQ(rfind_not_in_parentheses("(hello, world), (welcome, here),", ','), 31);
            CHECK_EQ(rfind_not_in_parentheses("(hello, world), (welcome, here)", ',', '[', ']'), 24);
            CHECK_EQ(rfind_not_in_parentheses("[hello, world](welcome, here)", ',', '(', ')'), 6);

            CHECK_EQ(find_not_in_parentheses("(hello, world,", ',').error(), ParseError::InvalidInput);
            CHECK_EQ(find_not_in_parentheses("(hello", ',').error(), ParseError::InvalidInput);

            static constexpr auto opens = std::array{ '[', '(' };
            static constexpr auto closes = std::array{ ']', ')' };
            CHECK_EQ(
                rfind_not_in_parentheses(",(hello, world), [welcome, here]", ',', opens, closes),
                15
            );
            CHECK_EQ(
                rfind_not_in_parentheses(",[welcome, here], ([(hello)], ([world]))", ',', opens, closes),
                16
            );
            CHECK_EQ(rfind_not_in_parentheses(",[hello, world](welcome, here)", ',', opens, closes), 0);
            CHECK_EQ(
                rfind_not_in_parentheses(",(hello, ]world,) welcome, here]", ',', opens, closes).error(),
                ParseError::InvalidInput
            );

            CHECK_EQ(rfind_not_in_parentheses("this, is, (a, string)", ',', opens, closes), 8);
            CHECK_EQ(rfind_not_in_parentheses(",this (a, string)", ',', opens, closes), 0);
            CHECK_EQ(rfind_not_in_parentheses("this (a, string)", ',', opens, closes), npos);
            CHECK_EQ(rfind_not_in_parentheses("(a, string)", ',', opens, closes), npos);
        }

        SUBCASE("Single char and similar open/close pair")
        {
            CHECK_EQ(rfind_not_in_parentheses(R"("some, csv")", ',', '"', '"'), npos);
            CHECK_EQ(rfind_not_in_parentheses(R"("some, csv","some, value")", ',', '"', '"'), 11);
            CHECK_EQ(rfind_not_in_parentheses(R"("some, csv","value""here")", ',', '"', '"'), 11);

            CHECK_EQ(
                find_not_in_parentheses(R"("some, csv)", ',', '"', '"').error(),
                ParseError::InvalidInput
            );

            static constexpr auto opens = std::array{ '[', '(', '\'', '"' };
            static constexpr auto closes = std::array{ ']', ')', '\'', '"' };
            CHECK_EQ(
                rfind_not_in_parentheses(R"(,[welcome, here], '("hello", world)')", ',', opens, closes),
                16
            );
            CHECK_EQ(
                rfind_not_in_parentheses(",[welcome, here], ('[(hello)], ([world])')", ',', opens, closes),
                16
            );
            CHECK_EQ(
                rfind_not_in_parentheses(",('hello', ']world,) welcome, here]", ',', opens, closes)
                    .error(),
                ParseError::InvalidInput
            );
        }

        SUBCASE("Substring and different open/close pair")
        {
            CHECK_EQ(rfind_not_in_parentheses("", "::"), npos);
            CHECK_EQ(rfind_not_in_parentheses("Nothing to see here", "::"), npos);
            CHECK_EQ(rfind_not_in_parentheses("(hello::world)", "::"), npos);

            CHECK_EQ(rfind_not_in_parentheses("hello::world", "::"), 5);
            CHECK_EQ(rfind_not_in_parentheses("hello::", "::"), 5);
            CHECK_EQ(rfind_not_in_parentheses("hello::world::welcome", "::"), 12);
            CHECK_EQ(rfind_not_in_parentheses("::(hello::world)::(welcome::here)", "::"), 16);
            CHECK_EQ(rfind_not_in_parentheses("(hello::world)::(welcome::here)", "::", '[', ']'), 24);
            CHECK_EQ(rfind_not_in_parentheses(",(welcome::here)[hello::world]", "::", '[', ']'), 9);

            CHECK_EQ(rfind_not_in_parentheses("hello::world::)", "::").error(), ParseError::InvalidInput);
            CHECK_EQ(rfind_not_in_parentheses("hello)", "::").error(), ParseError::InvalidInput);
            CHECK_EQ(rfind_not_in_parentheses("(hello", "::").error(), ParseError::InvalidInput);

            static constexpr auto opens = std::array{ '[', '(' };
            static constexpr auto closes = std::array{ ']', ')' };

            CHECK_EQ(
                rfind_not_in_parentheses("(some str)", "", opens, closes).error(),
                ParseError::InvalidInput
            );
            CHECK_EQ(
                rfind_not_in_parentheses(R"(hoy ,(hello , world), [welcome , here],elf)", " ,", opens, closes),
                3
            );
            CHECK_EQ(
                rfind_not_in_parentheses("hey ,([(hello)] , ([world])), [it , here]", " ,", opens, closes),
                3
            );
            CHECK_EQ(
                rfind_not_in_parentheses("(hello , ]world,) welcome, here],", ", ", opens, closes)
                    .error(),
                ParseError::InvalidInput
            );
        }

        SUBCASE("Substring and similar open/close pair")
        {
            CHECK_EQ(rfind_not_in_parentheses(R"("some::csv")", "::", '"', '"'), npos);
            CHECK_EQ(rfind_not_in_parentheses(R"("some::csv"::"some::value")", "::", '"', '"'), 11);
            CHECK_EQ(rfind_not_in_parentheses(R"("some::csv"::"value""here")", "::", '"', '"'), 11);
            CHECK_EQ(
                rfind_not_in_parentheses(R"(some::csv")", "::", '"', '"').error(),
                ParseError::InvalidInput
            );

            static constexpr auto opens = std::array{ '[', '(', '\'', '"' };
            static constexpr auto closes = std::array{ ']', ')', '\'', '"' };

            CHECK_EQ(
                rfind_not_in_parentheses("(some str)", "", opens, closes).error(),
                ParseError::InvalidInput
            );
            CHECK_EQ(
                find_not_in_parentheses(
                    "hoy , ('[(hello)] , ([world])'), [welcome , here]",
                    " ,",
                    opens,
                    closes
                ),
                3
            );
            CHECK_EQ(
                rfind_not_in_parentheses("('hello' , ']world,) welcome, here],", ", ", opens, closes)
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
