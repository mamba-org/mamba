// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/util/flat_bool_expr_tree.hpp"

using namespace mamba::util;

TEST_SUITE("flat_bool_expr_tree")
{
    TEST_CASE("Create bool postfix tokens")
    {
        // Infix:    (false and false) or (false or (false or true))
        // Postfix:  false true or false or false false and or
        auto postfix = std::vector<std::variant<bool, BoolOperator>>{
            false,
            true,
            BoolOperator::logical_or,
            false,
            BoolOperator::logical_or,
            false,
            false,
            BoolOperator::logical_and,
            BoolOperator::logical_or,
        };

        SUBCASE("Empty")
        {
            postfix.clear();
            const auto tree = flat_bool_expr_tree<bool>::from_postfix(postfix.cbegin(), postfix.cend());
            CHECK(tree.evaluate());
            CHECK_FALSE(tree.evaluate({}, false));
            CHECK(tree.evaluate([](auto b) { return !b; }));
            CHECK_FALSE(tree.evaluate([](auto b) { return !b; }, false));
        }

        SUBCASE("Evaluate tree")
        {
            const auto tree = flat_bool_expr_tree<bool>::from_postfix(postfix.cbegin(), postfix.cend());
            CHECK(tree.evaluate());
            CHECK(tree.evaluate([](auto b) { return !b; }));
        }
    }

    TEST_CASE("Create var postfix tokens")
    {
        // Infix:    (x0 or x1) and (x2 and (x3 or x4))
        const auto reference_eval = [](std::array<bool, 5> values) -> bool
        { return (values[0] || values[1]) && (values[2] && (values[3] || values[4])); };
        // Postfix:   x0 x1 or x2 x3 x4 or and and
        auto postfix = std::vector<std::variant<std::size_t, BoolOperator>>{
            0ul,
            1ul,
            BoolOperator::logical_or,
            2ul,
            3ul,
            4ul,
            BoolOperator::logical_or,
            BoolOperator::logical_and,
            BoolOperator::logical_and,
        };

        const auto tree = flat_bool_expr_tree<std::size_t>::from_postfix(
            postfix.cbegin(),
            postfix.cend()
        );

        for (bool x0 : { true, false })
        {
            for (bool x1 : { true, false })
            {
                for (bool x2 : { true, false })
                {
                    for (bool x3 : { true, false })
                    {
                        for (bool x4 : { true, false })
                        {
                            const auto values = std::array{ x0, x1, x2, x3, x4 };
                            const auto eval = [&values](std::size_t idx) { return values[idx]; };
                            CHECK_EQ(tree.evaluate(eval), reference_eval(values));
                        }
                    }
                }
            }
        }
    }
}
