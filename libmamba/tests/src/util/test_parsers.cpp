// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <catch2/catch_all.hpp>

#include "mamba/util/parsers.hpp"

using namespace mamba::util;

inline static constexpr auto npos = std::string_view::npos;

TEST_CASE("find_matching_parentheses")
{
    using Slice = std::pair<std::size_t, std::size_t>;

    SECTION("Different open/close pair")
    {
        REQUIRE(find_matching_parentheses("") == Slice(npos, npos));
        REQUIRE(find_matching_parentheses("Nothing to see here") == Slice(npos, npos));
        REQUIRE(find_matching_parentheses("(hello)", '[', ']') == Slice(npos, npos));

        REQUIRE(find_matching_parentheses("()") == Slice(0, 1));
        REQUIRE(find_matching_parentheses("hello()") == Slice(5, 6));
        REQUIRE(find_matching_parentheses("(hello)") == Slice(0, 6));
        REQUIRE(find_matching_parentheses("(hello (dear (sir))(!))(how(are(you)))") == Slice(0, 22));
        REQUIRE(find_matching_parentheses("[hello]", '[', ']') == Slice(0, 6));

        REQUIRE(find_matching_parentheses(")(").error() == ParseError::InvalidInput);
        REQUIRE(find_matching_parentheses("((hello)").error() == ParseError::InvalidInput);

        static constexpr auto opens = std::array{ '[', '(' };
        static constexpr auto closes = std::array{ ']', ')' };
        REQUIRE(find_matching_parentheses("([hello])", opens, closes) == Slice(0, 8));
        REQUIRE(find_matching_parentheses("(hello)[hello]", opens, closes) == Slice(0, 6));
    }

    SECTION("Similar open/close pair")
    {
        REQUIRE(find_matching_parentheses(R"("")", '"', '"') == Slice(0, 1));
        REQUIRE(find_matching_parentheses(R"("hello")", '"', '"') == Slice(0, 6));
        REQUIRE(find_matching_parentheses(R"("some","csv","value")", '"', '"') == Slice(0, 5));
        REQUIRE(
            find_matching_parentheses(R"(Here is "some)", '"', '"').error() == ParseError::InvalidInput
        );

        static constexpr auto opens = std::array{ '[', '(', '\'' };
        static constexpr auto closes = std::array{ ']', ')', '\'' };
        REQUIRE(find_matching_parentheses("'[hello]'", opens, closes) == Slice(0, 8));
        REQUIRE(find_matching_parentheses("hello['hello', 'world']", opens, closes) == Slice(5, 22));
    }
}

TEST_CASE("rfind_matching_parentheses")
{
    using Slice = std::pair<std::size_t, std::size_t>;

    SECTION("Different open/close pair")
    {
        REQUIRE(rfind_matching_parentheses("") == Slice(npos, npos));
        REQUIRE(rfind_matching_parentheses("Nothing to see here") == Slice(npos, npos));
        REQUIRE(rfind_matching_parentheses("(hello)", '[', ']') == Slice(npos, npos));

        REQUIRE(rfind_matching_parentheses("()") == Slice(0, 1));
        REQUIRE(rfind_matching_parentheses("hello()") == Slice(5, 6));
        REQUIRE(rfind_matching_parentheses("(hello)dear") == Slice(0, 6));
        REQUIRE(rfind_matching_parentheses("(hello (dear (sir))(!))(how(are(you)))") == Slice(23, 37));
        REQUIRE(rfind_matching_parentheses("[hello]", '[', ']') == Slice(0, 6));

        REQUIRE(rfind_matching_parentheses(")(").error() == ParseError::InvalidInput);
        REQUIRE(rfind_matching_parentheses("(hello))").error() == ParseError::InvalidInput);

        static constexpr auto opens = std::array{ '[', '(' };
        static constexpr auto closes = std::array{ ']', ')' };
        REQUIRE(rfind_matching_parentheses("([hello])", opens, closes) == Slice(0, 8));
        REQUIRE(rfind_matching_parentheses("(hello)[hello]", opens, closes) == Slice(7, 13));
    }

    SECTION("Similar open/close pair")
    {
        REQUIRE(rfind_matching_parentheses(R"("")", '"', '"') == Slice(0, 1));
        REQUIRE(rfind_matching_parentheses(R"("hello")", '"', '"') == Slice(0, 6));
        REQUIRE(rfind_matching_parentheses(R"("some","csv","value")", '"', '"') == Slice(13, 19));
        REQUIRE(
            rfind_matching_parentheses(R"(Here is "some)", '"', '"').error() == ParseError::InvalidInput
        );

        static constexpr auto opens = std::array{ '[', '(', '\'' };
        static constexpr auto closes = std::array{ ']', ')', '\'' };
        REQUIRE(rfind_matching_parentheses("'[hello]'", opens, closes) == Slice(0, 8));
        REQUIRE(rfind_matching_parentheses("['hello', 'world']dear", opens, closes) == Slice(0, 17));
    }
}

TEST_CASE("find_not_in_parentheses")
{
    SECTION("Single char and different open/close pair")
    {
        REQUIRE(find_not_in_parentheses("", ',') == npos);
        REQUIRE(find_not_in_parentheses("Nothing to see here", ',') == npos);
        REQUIRE(find_not_in_parentheses("(hello, world)", ',') == npos);

        REQUIRE(find_not_in_parentheses("hello, world", ',') == 5);
        REQUIRE(find_not_in_parentheses("hello, world, welcome", ',') == 5);
        REQUIRE(find_not_in_parentheses("(hello, world), (welcome, here),", ',') == 14);
        REQUIRE(find_not_in_parentheses("(hello, world), (welcome, here),", ',', '[', ']') == 6);
        REQUIRE(find_not_in_parentheses("[hello, world](welcome, here),", ',', '[', ']') == 22);

        REQUIRE(find_not_in_parentheses("(hello, world,", ',').error() == ParseError::InvalidInput);
        REQUIRE(find_not_in_parentheses("(hello", ',').error() == ParseError::InvalidInput);

        static constexpr auto opens = std::array{ '[', '(' };
        static constexpr auto closes = std::array{ ']', ')' };
        REQUIRE(find_not_in_parentheses("(hello, world), [welcome, here],", ',', opens, closes) == 14);
        REQUIRE(
            find_not_in_parentheses("([(hello)], ([world])), [welcome, here],", ',', opens, closes)
            == 22
        );
        REQUIRE(find_not_in_parentheses("[hello, world](welcome, here),", ',', opens, closes) == 29);
        REQUIRE(
            find_not_in_parentheses("(hello, ]world,) welcome, here],", ',', opens, closes).error()
            == ParseError::InvalidInput
        );

        // The following unfortunately does not work as we would need to allocate a stack
        // to keep track of the opening and closing of parentheses.
        // REQUIRE(
        //     find_not_in_parentheses("(hello, [world, )welcome, here],", ',', opens,
        //     closes).error() == ParseError::InvalidInput
        // );
    }

    SECTION("Single char and similar open/close pair")
    {
        REQUIRE(find_not_in_parentheses(R"("some, csv")", ',', '"', '"') == npos);

        REQUIRE(find_not_in_parentheses(R"("some, csv",value)", ',', '"', '"') == 11);
        REQUIRE(find_not_in_parentheses(R"("some, csv""value","here")", ',', '"', '"') == 18);

        REQUIRE(
            find_not_in_parentheses(R"("some, csv)", ',', '"', '"').error() == ParseError::InvalidInput
        );

        static constexpr auto opens = std::array{ '[', '(', '\'', '"' };
        static constexpr auto closes = std::array{ ']', ')', '\'', '"' };
        REQUIRE(
            find_not_in_parentheses(R"('("hello", world)', [welcome, here],)", ',', opens, closes) == 18
        );
        REQUIRE(
            find_not_in_parentheses("('[(hello)], ([world])'), [welcome, here],", ',', opens, closes)
            == 24
        );
        REQUIRE(
            find_not_in_parentheses("('hello', ']world,) welcome, here],", ',', opens, closes).error()
            == ParseError::InvalidInput
        );
    }

    SECTION("Substring and different open/close pair")
    {
        REQUIRE(find_not_in_parentheses("", "::") == npos);
        REQUIRE(find_not_in_parentheses("Nothing to see here", "::") == npos);
        REQUIRE(find_not_in_parentheses("(hello::world)", "::") == npos);

        REQUIRE(find_not_in_parentheses("hello::world", "::") == 5);
        REQUIRE(find_not_in_parentheses("hello::world::welcome", "::") == 5);
        REQUIRE(find_not_in_parentheses("(hello::world)::(welcome::here)::", "::").value() == 14);
        REQUIRE(find_not_in_parentheses("(hello::world)::(welcome::here)", "::", '[', ']') == 6);
        REQUIRE(find_not_in_parentheses("[hello::world](welcome::here),", "::", '[', ']') == 22);
        //
        REQUIRE(find_not_in_parentheses("(hello::world::", "::").error() == ParseError::InvalidInput);
        REQUIRE(find_not_in_parentheses("(hello", "::").error() == ParseError::InvalidInput);

        static constexpr auto opens = std::array{ '[', '(' };
        static constexpr auto closes = std::array{ ']', ')' };

        REQUIRE(
            find_not_in_parentheses("(some str)", "", opens, closes).error() == ParseError::InvalidInput
        );
        REQUIRE(
            find_not_in_parentheses(R"((hello , world), [welcome , here] ,elf)", " ,", opens, closes)
            == 33
        );
        REQUIRE(
            find_not_in_parentheses("([(hello)] , ([world])), [welcome , here] ,elf", " ,", opens, closes)
            == 41
        );
        REQUIRE(
            find_not_in_parentheses("(hello , ]world,) welcome, here],", ", ", opens, closes).error()
            == ParseError::InvalidInput
        );
    }

    SECTION("Substring and similar open/close pair")
    {
        REQUIRE(find_not_in_parentheses(R"("some::csv")", "::", '"', '"') == npos);

        REQUIRE(find_not_in_parentheses(R"("some::csv"::value)", "::", '"', '"') == 11);
        REQUIRE(find_not_in_parentheses(R"("some::csv""value"::"here")", "::", '"', '"') == 18);

        REQUIRE(
            find_not_in_parentheses(R"("some::csv)", "::", '"', '"').error() == ParseError::InvalidInput
        );

        static constexpr auto opens = std::array{ '[', '(', '\'', '"' };
        static constexpr auto closes = std::array{ ']', ')', '\'', '"' };

        REQUIRE(
            find_not_in_parentheses("(some str)", "", opens, closes).error() == ParseError::InvalidInput
        );
        REQUIRE(
            find_not_in_parentheses(R"('("hello" , world)', [welcome , here] ,elf)", " ,", opens, closes)
            == 37
        );
        REQUIRE(
            find_not_in_parentheses("('[(hello)] , ([world])'), [welcome , here] ,elf", " ,", opens, closes)
            == 43
        );
        REQUIRE(
            find_not_in_parentheses("('hello' , ']world,) welcome, here],", ", ", opens, closes).error()
            == ParseError::InvalidInput
        );
    }
}

TEST_CASE("rfind_not_in_parentheses")
{
    SECTION("Single char and different open/close pair")
    {
        REQUIRE(rfind_not_in_parentheses("", ',') == npos);
        REQUIRE(rfind_not_in_parentheses("Nothing to see here", ',') == npos);
        REQUIRE(rfind_not_in_parentheses("(hello, world)", ',') == npos);

        REQUIRE(rfind_not_in_parentheses("hello, world", ',') == 5);
        REQUIRE(rfind_not_in_parentheses("hello, world, welcome", ',') == 12);
        REQUIRE(rfind_not_in_parentheses("(hello, world), (welcome, here),", ',') == 31);
        REQUIRE(rfind_not_in_parentheses("(hello, world), (welcome, here)", ',', '[', ']') == 24);
        REQUIRE(rfind_not_in_parentheses("[hello, world](welcome, here)", ',', '(', ')') == 6);

        REQUIRE(find_not_in_parentheses("(hello, world,", ',').error() == ParseError::InvalidInput);
        REQUIRE(find_not_in_parentheses("(hello", ',').error() == ParseError::InvalidInput);

        static constexpr auto opens = std::array{ '[', '(' };
        static constexpr auto closes = std::array{ ']', ')' };
        REQUIRE(rfind_not_in_parentheses(",(hello, world), [welcome, here]", ',', opens, closes) == 15);
        REQUIRE(
            rfind_not_in_parentheses(",[welcome, here], ([(hello)], ([world]))", ',', opens, closes)
            == 16
        );
        REQUIRE(rfind_not_in_parentheses(",[hello, world](welcome, here)", ',', opens, closes) == 0);
        REQUIRE(
            rfind_not_in_parentheses(",(hello, ]world,) welcome, here]", ',', opens, closes).error()
            == ParseError::InvalidInput
        );

        REQUIRE(rfind_not_in_parentheses("this, is, (a, string)", ',', opens, closes) == 8);
        REQUIRE(rfind_not_in_parentheses(",this (a, string)", ',', opens, closes) == 0);
        REQUIRE(rfind_not_in_parentheses("this (a, string)", ',', opens, closes) == npos);
        REQUIRE(rfind_not_in_parentheses("(a, string)", ',', opens, closes) == npos);
    }

    SECTION("Single char and similar open/close pair rfind")
    {
        REQUIRE(rfind_not_in_parentheses(R"("some, csv")", ',', '"', '"') == npos);
        REQUIRE(rfind_not_in_parentheses(R"("some, csv","some, value")", ',', '"', '"') == 11);
        REQUIRE(rfind_not_in_parentheses(R"("some, csv","value""here")", ',', '"', '"') == 11);

        REQUIRE(
            find_not_in_parentheses(R"("some, csv)", ',', '"', '"').error() == ParseError::InvalidInput
        );

        static constexpr auto opens = std::array{ '[', '(', '\'', '"' };
        static constexpr auto closes = std::array{ ']', ')', '\'', '"' };
        REQUIRE(
            rfind_not_in_parentheses(R"(,[welcome, here], '("hello", world)')", ',', opens, closes)
            == 16
        );
        REQUIRE(
            rfind_not_in_parentheses(",[welcome, here], ('[(hello)], ([world])')", ',', opens, closes)
            == 16
        );
        REQUIRE(
            rfind_not_in_parentheses(",('hello', ']world,) welcome, here]", ',', opens, closes).error()
            == ParseError::InvalidInput
        );
    }

    SECTION("Substring and different open/close pair")
    {
        REQUIRE(rfind_not_in_parentheses("", "::") == npos);
        REQUIRE(rfind_not_in_parentheses("Nothing to see here", "::") == npos);
        REQUIRE(rfind_not_in_parentheses("(hello::world)", "::") == npos);

        REQUIRE(rfind_not_in_parentheses("hello::world", "::") == 5);
        REQUIRE(rfind_not_in_parentheses("hello::", "::") == 5);
        REQUIRE(rfind_not_in_parentheses("hello::world::welcome", "::") == 12);
        REQUIRE(rfind_not_in_parentheses("::(hello::world)::(welcome::here)", "::") == 16);
        REQUIRE(rfind_not_in_parentheses("(hello::world)::(welcome::here)", "::", '[', ']') == 24);
        REQUIRE(rfind_not_in_parentheses(",(welcome::here)[hello::world]", "::", '[', ']') == 9);

        REQUIRE(rfind_not_in_parentheses("hello::world::)", "::").error() == ParseError::InvalidInput);
        REQUIRE(rfind_not_in_parentheses("hello)", "::").error() == ParseError::InvalidInput);
        REQUIRE(rfind_not_in_parentheses("(hello", "::").error() == ParseError::InvalidInput);

        static constexpr auto opens = std::array{ '[', '(' };
        static constexpr auto closes = std::array{ ']', ')' };

        REQUIRE(
            rfind_not_in_parentheses("(some str)", "", opens, closes).error() == ParseError::InvalidInput
        );
        REQUIRE(
            rfind_not_in_parentheses(R"(hoy ,(hello , world), [welcome , here],elf)", " ,", opens, closes)
            == 3
        );
        REQUIRE(
            rfind_not_in_parentheses("hey ,([(hello)] , ([world])), [it , here]", " ,", opens, closes)
            == 3
        );
        REQUIRE(
            rfind_not_in_parentheses("(hello , ]world,) welcome, here],", ", ", opens, closes).error()
            == ParseError::InvalidInput
        );
    }

    SECTION("Substring and similar open/close pair")
    {
        REQUIRE(rfind_not_in_parentheses(R"("some::csv")", "::", '"', '"') == npos);
        REQUIRE(rfind_not_in_parentheses(R"("some::csv"::"some::value")", "::", '"', '"') == 11);
        REQUIRE(rfind_not_in_parentheses(R"("some::csv"::"value""here")", "::", '"', '"') == 11);
        REQUIRE(
            rfind_not_in_parentheses(R"(some::csv")", "::", '"', '"').error() == ParseError::InvalidInput
        );

        static constexpr auto opens = std::array{ '[', '(', '\'', '"' };
        static constexpr auto closes = std::array{ ']', ')', '\'', '"' };

        REQUIRE(
            rfind_not_in_parentheses("(some str)", "", opens, closes).error() == ParseError::InvalidInput
        );
        REQUIRE(
            find_not_in_parentheses("hoy , ('[(hello)] , ([world])'), [welcome , here]", " ,", opens, closes)
            == 3
        );
        REQUIRE(
            rfind_not_in_parentheses("('hello' , ']world,) welcome, here],", ", ", opens, closes).error()
            == ParseError::InvalidInput
        );
    }
}

TEST_CASE("glob_match")
{
    REQUIRE(glob_match("python", "python"));
    REQUIRE_FALSE(glob_match("cpython", "python"));
    REQUIRE_FALSE(glob_match("python", "cpython"));
    REQUIRE_FALSE(glob_match("python", ""));

    REQUIRE(glob_match("py*", "py"));
    REQUIRE(glob_match("py*", "py"));
    REQUIRE(glob_match("py*", "python"));
    REQUIRE_FALSE(glob_match("py*", "cpython"));
    REQUIRE_FALSE(glob_match("py*", ""));

    REQUIRE(glob_match("*37", "python37"));
    REQUIRE(glob_match("*37", "37"));
    REQUIRE_FALSE(glob_match("*37", "python37-linux64"));
    REQUIRE_FALSE(glob_match("*37", ""));

    REQUIRE(glob_match("*py*", "python"));
    REQUIRE(glob_match("*py*", "cpython"));
    REQUIRE(glob_match("*py*", "cpy"));
    REQUIRE_FALSE(glob_match("*py*", "linux"));
    REQUIRE_FALSE(glob_match("*py*", ""));

    REQUIRE(glob_match("*py*-3*-*-64", "cpython-37-linux-64"));
    REQUIRE(glob_match("*py*-3*-*-64", "python-37-more-linux-64"));
    REQUIRE_FALSE(glob_match("*py*-3*-*-64", "cpython-37-linux-64-more"));
    REQUIRE_FALSE(glob_match("*py*-3*-*-64", ""));

    REQUIRE(glob_match("py**", "python"));
    REQUIRE_FALSE(glob_match("py**", "cpython"));
    REQUIRE(glob_match("**37", "python37"));
    REQUIRE_FALSE(glob_match("**37", "python37-linux64"));
    REQUIRE(glob_match("**py**", "python"));
    REQUIRE_FALSE(glob_match("**py**", "linux"));
}
