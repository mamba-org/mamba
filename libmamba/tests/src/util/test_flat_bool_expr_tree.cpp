// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/util/flat_bool_expr_tree.hpp"

#include "doctest-printer/array.hpp"

using namespace mamba::util;

TEST_SUITE("util::flat_bool_expr_tree")
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
        auto parser = PostfixParser<char, std::string>{};

        SUBCASE("empty")
        {
            CHECK(parser.finalize());
            const auto& tree = parser.tree();
            CHECK(tree.empty());
        }

        SUBCASE("a")
        {
            CHECK(parser.push_variable('a'));
            CHECK(parser.finalize());

            const auto& tree = parser.tree();
            CHECK_EQ(tree.size(), 1);
            REQUIRE(tree.is_leaf(0));
            CHECK_EQ(tree.leaf(0), 'a');
            CHECK_EQ(tree.root(), 0);
        }

        SUBCASE("a b + c d e + * *")
        {
            // Infix:   (a + b) * (c * (d + e))
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_variable('b'));
            CHECK(parser.push_operator("+"));
            CHECK(parser.push_variable('c'));
            CHECK(parser.push_variable('d'));
            CHECK(parser.push_variable('e'));
            CHECK(parser.push_operator("+"));
            CHECK(parser.push_operator("*"));
            CHECK(parser.push_operator("*"));
            CHECK(parser.finalize());

            const auto& tree = parser.tree();
            CHECK_EQ(tree.size(), 9);

            const auto visited = visit_all_once_no_cycle(tree);
            CHECK_EQ(visited.size(), tree.size());
        }

        SUBCASE("a b")
        {
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_variable('b'));
            CHECK_FALSE(parser.finalize());
        }

        SUBCASE("+")
        {
            CHECK_FALSE(parser.push_operator("+"));
        }

        SUBCASE("a b + *")
        {
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_variable('b'));
            CHECK(parser.push_operator("+"));
            CHECK_FALSE(parser.push_operator("*"));
        }

        SUBCASE("a b + c")
        {
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_variable('b'));
            CHECK(parser.push_operator("+"));
            CHECK(parser.push_variable('c'));
            CHECK_FALSE(parser.finalize());
        }
    }

    TEST_CASE("InfixParser")
    {
        auto parser = InfixParser<char, std::string>{};

        SUBCASE("empty")
        {
            CHECK(parser.finalize());
            const auto& tree = parser.tree();
            CHECK(tree.empty());
        }

        SUBCASE("(((a)))")
        {
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.finalize());

            const auto& tree = parser.tree();
            REQUIRE_EQ(tree.size(), 1);
            REQUIRE(tree.is_leaf(0));
            CHECK_EQ(tree.root(), 0);
            CHECK_EQ(tree.leaf(0), 'a');
        }

        SUBCASE("(((a)) + b)")
        {
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.push_operator("+"));
            CHECK(parser.push_variable('b'));
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.finalize());

            const auto& tree = parser.tree();
            REQUIRE_EQ(tree.size(), 3);
            const auto root = tree.root();
            REQUIRE(tree.is_branch(root));
            CHECK_EQ(tree.branch(root), "+");
            REQUIRE(tree.is_leaf(tree.left(root)));
            CHECK_EQ(tree.leaf(tree.left(root)), 'a');
            REQUIRE(tree.is_leaf(tree.right(root)));
            CHECK_EQ(tree.leaf(tree.right(root)), 'b');
        }

        SUBCASE("(a + b) * (c * (d + e))")
        {
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_operator("+"));
            CHECK(parser.push_variable('b'));
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.push_operator("*"));
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_variable('c'));
            CHECK(parser.push_operator("*"));
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_variable('d'));
            CHECK(parser.push_operator("+"));
            CHECK(parser.push_variable('e'));
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.finalize());

            const auto& tree = parser.tree();
            CHECK_EQ(tree.size(), 9);

            const auto visited = visit_all_once_no_cycle(tree);
            CHECK_EQ(visited.size(), tree.size());
        }

        SUBCASE("(")
        {
            CHECK(parser.push_left_parenthesis());
            CHECK_FALSE(parser.finalize());
        }

        SUBCASE(")")
        {
            CHECK_FALSE(parser.push_right_parenthesis());
        }

        SUBCASE("(a+b")
        {
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_operator("+"));
            CHECK(parser.push_variable('b'));
            CHECK_FALSE(parser.finalize());
        }

        SUBCASE("a)")
        {
            CHECK(parser.push_variable('a'));
            CHECK_FALSE(parser.push_right_parenthesis());
        }

        SUBCASE("+")
        {
            CHECK_FALSE(parser.push_operator("+"));
        }

        SUBCASE("a b +")
        {
            CHECK(parser.push_variable('a'));
            CHECK_FALSE(parser.push_variable('b'));
        }

        SUBCASE("a + + b")
        {
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_operator("+"));
            CHECK_FALSE(parser.push_operator("+"));
        }

        SUBCASE("a +")
        {
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_operator("+"));
            CHECK_FALSE(parser.finalize());
        }

        SUBCASE("a + )")
        {
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_operator("+"));
            CHECK_FALSE(parser.push_right_parenthesis());
        }
        SUBCASE("(((a)) + b (* c")
        {
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_left_parenthesis());
            CHECK(parser.push_variable('a'));
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.push_right_parenthesis());
            CHECK(parser.push_operator("+"));
            CHECK(parser.push_variable('b'));
            CHECK_FALSE(parser.push_left_parenthesis());
        }
    }

    TEST_CASE("Bool postfix tokens")
    {
        // Infix:    (false and false) or (false or (false or true))
        // Postfix:  false true or false or false false and or
        auto parser = PostfixParser<bool, BoolOperator>{};
        CHECK(parser.push_variable(false));
        CHECK(parser.push_variable(true));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(false));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(false));
        CHECK(parser.push_variable(false));
        CHECK(parser.push_operator(BoolOperator::logical_and));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        auto tree = flat_bool_expr_tree(std::move(parser).tree());

        SUBCASE("Empty")
        {
            tree.clear();
            CHECK(tree.evaluate());
            CHECK_FALSE(tree.evaluate({}, false));
            CHECK(tree.evaluate([](auto b) { return !b; }));
            CHECK_FALSE(tree.evaluate([](auto b) { return !b; }, false));
        }

        SUBCASE("Evaluate tree")
        {
            CHECK(tree.evaluate());
            CHECK(tree.evaluate([](auto b) { return !b; }));
        }
    }

    namespace
    {
        /**
         * Convert a bool to a boolean bit set.
         *
         * The output is inversed, so that the little end of the integer is the first element
         * of the output bit set.
         */
        template <typename std::size_t N, typename I>
        constexpr auto integer_to_bools(I x) -> std::array<bool, N>
        {
            std::array<bool, N> out = {};
            for (std::size_t i = 0; i < N; ++i)
            {
                out[i] = ((x >> i) & I(1)) == I(1);
            }
            return out;
        }
    }

    TEST_CASE("Test exponential boolean cross-product")
    {
        CHECK_EQ(integer_to_bools<5>(0b00000), std::array{ false, false, false, false, false });
        CHECK_EQ(integer_to_bools<4>(0b1111), std::array{ true, true, true, true });
        CHECK_EQ(
            integer_to_bools<7>(0b1001101),
            std::array{ true, false, true, true, false, false, true }
        );
    }

    TEST_CASE("Create var postfix tokens")
    {
        const auto reference_eval = [](std::array<bool, 5> x) -> bool
        { return (x[0] || x[1]) && (x[2] && (x[3] || x[4])); };
        // Infix:     ((x3 or x4) and x2) and (x0 or x1)
        // Postfix:   x0 x1 or x2 x3 x4 or and and
        auto parser = PostfixParser<std::size_t, BoolOperator>{};
        CHECK(parser.push_variable(0));
        CHECK(parser.push_variable(1));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(2));
        CHECK(parser.push_variable(3));
        CHECK(parser.push_variable(4));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_operator(BoolOperator::logical_and));
        CHECK(parser.push_operator(BoolOperator::logical_and));
        auto tree = flat_bool_expr_tree(std::move(parser).tree());

        static constexpr std::size_t n_vars = 5;
        for (std::size_t x = 0; x < (1 << n_vars); ++x)
        {
            const auto values = integer_to_bools<n_vars>(x);
            const auto eval = [&values](std::size_t idx) { return values[idx]; };
            CHECK_EQ(tree.evaluate(eval), reference_eval(values));
        }
    }

    TEST_CASE("Create var infix tokens")
    {
        const auto reference_eval = [](std::array<bool, 7> x) -> bool
        { return ((x[0] || x[1]) && (x[2] || x[3] || x[4]) && x[5]) || x[6]; };
        auto parser = InfixParser<std::size_t, BoolOperator>{};
        // Infix:  ((x0 or x1) and (x2 or x3 or x4) and x5) or x6
        CHECK(parser.push_left_parenthesis());
        CHECK(parser.push_left_parenthesis());
        CHECK(parser.push_variable(0));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(1));
        CHECK(parser.push_right_parenthesis());
        CHECK(parser.push_operator(BoolOperator::logical_and));
        CHECK(parser.push_left_parenthesis());
        CHECK(parser.push_variable(2));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(3));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(4));
        CHECK(parser.push_right_parenthesis());
        CHECK(parser.push_operator(BoolOperator::logical_and));
        CHECK(parser.push_variable(5));
        CHECK(parser.push_right_parenthesis());
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(6));
        CHECK(parser.finalize());
        auto tree = flat_bool_expr_tree(std::move(parser).tree());

        static constexpr std::size_t n_vars = 7;
        for (std::size_t x = 0; x < (1 << n_vars); ++x)
        {
            const auto values = integer_to_bools<n_vars>(x);
            CAPTURE(values);
            const auto eval = [&values](std::size_t idx) { return values[idx]; };
            CHECK_EQ(tree.evaluate(eval), reference_eval(values));
        }
    }

    TEST_CASE("Infix traversal")
    {
        auto parser = InfixParser<std::size_t, BoolOperator>{};
        // Infix:  ((x0 or x1) and (x2 or x3 or x4) and x5) or x6
        CHECK(parser.push_left_parenthesis());
        CHECK(parser.push_left_parenthesis());
        CHECK(parser.push_variable(0));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(1));
        CHECK(parser.push_right_parenthesis());
        CHECK(parser.push_operator(BoolOperator::logical_and));
        CHECK(parser.push_left_parenthesis());
        CHECK(parser.push_variable(2));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(3));
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(4));
        CHECK(parser.push_right_parenthesis());
        CHECK(parser.push_operator(BoolOperator::logical_and));
        CHECK(parser.push_variable(5));
        CHECK(parser.push_right_parenthesis());
        CHECK(parser.push_operator(BoolOperator::logical_or));
        CHECK(parser.push_variable(6));
        CHECK(parser.finalize());
        auto tree = flat_bool_expr_tree(std::move(parser).tree());

        auto result = std::string();
        tree.infix_for_each(
            [&](const auto& token)
            {
                using tree_type = decltype(tree);
                using Token = std::decay_t<decltype(token)>;
                if constexpr (std::is_same_v<Token, tree_type::LeftParenthesis>)
                {
                    result += '(';
                }
                if constexpr (std::is_same_v<Token, tree_type::RightParenthesis>)
                {
                    result += ')';
                }
                if constexpr (std::is_same_v<Token, BoolOperator>)
                {
                    result += (token == BoolOperator::logical_or) ? " or " : " and ";
                }
                if constexpr (std::is_same_v<Token, tree_type::variable_type>)
                {
                    result += 'x';
                    result += std::to_string(token);
                }
            }
        );
        // There could be many representations, here is one
        CHECK_EQ(result, "((x0 or x1) and ((x2 or (x3 or x4)) and x5)) or x6");
    }
}
