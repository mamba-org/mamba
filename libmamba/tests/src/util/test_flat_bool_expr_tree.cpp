// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/util/flat_bool_expr_tree.hpp"

using namespace mamba::util;

TEST_SUITE("flat_bool_expr_tree")
{
    TEST_CASE("flat_binary_tree")
    {
        auto tree = flat_binary_tree<std::string, int>{};
        CHECK(tree.empty());
        CHECK_EQ(tree.size(), 0);

        SUBCASE("Add nodes")
        {
            const auto l1 = tree.add_leaf(1);
            CHECK(tree.is_leaf(l1));
            CHECK_FALSE(tree.is_branch(l1));
            CHECK_EQ(tree.leaf(l1), 1);
            CHECK_EQ(tree.root(), l1);

            const auto l2 = tree.add_leaf(2);
            CHECK(tree.is_leaf(l2));
            CHECK_FALSE(tree.is_branch(l2));
            CHECK_EQ(tree.leaf(l2), 2);

            const auto pa = tree.add_branch("a", l1, l2);
            CHECK_FALSE(tree.is_leaf(pa));
            CHECK(tree.is_branch(pa));
            CHECK_EQ(tree.branch(pa), "a");
            CHECK_EQ(tree.left(pa), l1);
            CHECK_EQ(tree.right(pa), l2);
            CHECK_EQ(tree.root(), pa);

            const auto l3 = tree.add_leaf(3);
            CHECK(tree.is_leaf(l3));
            CHECK_FALSE(tree.is_branch(l3));
            CHECK_EQ(tree.leaf(l2), 2);

            const auto pb = tree.add_branch("b", pa, l3);
            CHECK_FALSE(tree.is_leaf(pb));
            CHECK(tree.is_branch(pb));
            CHECK_EQ(tree.branch(pb), "b");
            CHECK_EQ(tree.left(pb), pa);
            CHECK_EQ(tree.right(pb), l3);
            CHECK_EQ(tree.root(), pb);

            CHECK_FALSE(tree.empty());
            CHECK_EQ(tree.size(), 5);

            SUBCASE("Clear nodes")
            {
                tree.clear();
                CHECK(tree.empty());
                CHECK_EQ(tree.size(), 0);
            }
        }
    }

    namespace
    {
        template <typename Tree, typename Queue>
        void visit_all_once_no_cycle_impl(const Tree& tree, Queue& q, std::size_t idx)
        {
            if (std::find(q.cbegin(), q.cend(), idx) != q.cend())
            {
                throw std::invalid_argument("Tree has cycle");
            }
            q.push_back(idx);
            if (tree.is_branch(idx))
            {
                visit_all_once_no_cycle_impl(tree, q, tree.left(idx));
                visit_all_once_no_cycle_impl(tree, q, tree.right(idx));
            }
        }

        template <typename Tree>
        auto visit_all_once_no_cycle(const Tree& tree)
        {
            auto visited = std::vector<std::size_t>{};
            visited.reserve(tree.size());
            if (!tree.empty())
            {
                visit_all_once_no_cycle_impl(tree, visited, tree.size() - 1);
            }
            std::sort(visited.begin(), visited.end());
            return visited;
        }
    }

    TEST_CASE("PostfixParser")
    {
        // Infix:   (a + b) * (c * (d + e))
        // Postfix: a b + c d e + * *
        auto parser = PostfixParser<char, std::string>{};
        parser.push_variable('a');
        parser.push_variable('b');
        parser.push_operator("+");
        parser.push_variable('c');
        parser.push_variable('d');
        parser.push_variable('e');
        parser.push_operator("+");
        parser.push_operator("*");
        parser.push_operator("*");

        const auto& tree = parser.tree();
        CHECK_EQ(tree.size(), 9);

        const auto visited = visit_all_once_no_cycle(tree);
        CHECK_EQ(visited.size(), tree.size());
    }

    TEST_CASE("Bool postfix tokens")
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
