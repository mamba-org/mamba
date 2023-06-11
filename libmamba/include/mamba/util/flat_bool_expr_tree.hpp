// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_FLAT_EXPR_TREE_HPP
#define MAMBA_UTIL_FLAT_EXPR_TREE_HPP

#include <functional>
#include <utility>
#include <variant>
#include <vector>

namespace mamba::util
{
    // TODO C++20 this is std::identity
    struct identity
    {
        template <typename T>
        constexpr auto operator()(T&& t) const noexcept -> T&&;
    };

    enum struct BoolOperator
    {
        logical_and,
        logical_or
    };

    template <typename Variable>
    class flat_bool_expr_tree
    {
    public:

        using self_type = flat_bool_expr_tree<Variable>;
        using operator_type = BoolOperator;
        using variable_type = Variable;

        template <typename Iter>
        [[nodiscard]] static auto from_postfix(Iter first, Iter last) -> self_type;

        flat_bool_expr_tree() = default;

        template <typename UnaryFunc = identity>
        [[nodiscard]] auto evaluate(UnaryFunc&& var_evaluator = {}, bool empty_val = true) const
            -> bool;

    private:

        struct OperatorNode
        {
            operator_type op;
            std::size_t left_expr = 0;
            std::size_t right_expr = 0;
        };
        using node_type = std::variant<OperatorNode, variable_type>;
        using node_list = std::vector<node_type>;

        template <typename Iter>
        static auto postfix_tokens_to_nodes(Iter first, Iter last) -> node_list;
        static auto from_postfix_impl(node_list& nodes, std::size_t idx) -> std::size_t;

        flat_bool_expr_tree(node_list&& nodes);

        template <typename UnaryFunc>
        auto evaluate_impl(UnaryFunc& var_evaluator, std::size_t idx) const -> bool;

        node_list m_nodes = {};
    };
}

#include <cassert>
#include <iterator>
#include <type_traits>


namespace mamba::util
{

    /********************************
     *  Implementation of identity  *
     ********************************/

    template <typename T>
    constexpr auto identity::operator()(T&& t) const noexcept -> T&&
    {
        return std::forward<T>(t);
    }

    /*******************************************
     *  Implementation of flat_bool_expr_tree  *
     *******************************************/

    template <typename V>
    flat_bool_expr_tree<V>::flat_bool_expr_tree(node_list&& nodes)
        : m_nodes(std::move(nodes))
    {
    }

    template <typename V>
    template <typename Iter>
    auto flat_bool_expr_tree<V>::postfix_tokens_to_nodes(Iter first, Iter last) -> node_list
    {
        node_list out = {};
        {
            const auto n_tokens = std::distance(first, last);
            assert(n_tokens >= 0);
            out.reserve(static_cast<std::size_t>(n_tokens));
        }
        for (; first != last; ++first)
        {
            out.push_back(std::visit(
                [&](const auto& token) -> node_type
                {
                    using Token = std::decay_t<decltype(token)>;
                    if constexpr (std::is_same_v<Token, variable_type>)
                    {
                        return { token };
                    }
                    else  // std::is_same_v<Token, operator_type>
                    {
                        return { OperatorNode{ token } };
                    }
                },
                *first
            ));
        }
        return out;
    }

    template <typename V>
    auto flat_bool_expr_tree<V>::from_postfix_impl(node_list& nodes, std::size_t idx) -> std::size_t
    {
        assert(idx < nodes.size());
        return std::visit(
            [&](auto& node) -> std::size_t
            {
                using Node = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<Node, variable_type>)
                {
                    return idx;
                }
                else  // std::is_same_v<Node, OperatorNode>
                {
                    assert(idx >= 1);
                    node.right_expr = idx - 1;
                    auto const right_end = from_postfix_impl(nodes, node.right_expr);
                    assert(right_end >= 1);
                    node.left_expr = right_end - 1;
                    auto const left_end = from_postfix_impl(nodes, node.left_expr);
                    return left_end;
                }
            },
            nodes[idx]
        );
    }

    template <typename V>
    template <typename Iter>
    auto flat_bool_expr_tree<V>::from_postfix(Iter first, Iter last) -> self_type
    {
        if (first == last)
        {
            return self_type{};
        }
        node_list out = postfix_tokens_to_nodes(first, last);
        assert(!out.empty());
        from_postfix_impl(out, out.size() - 1);
        return self_type{ std::move(out) };
    }

    template <typename V>
    template <typename UnaryFunc>
    auto flat_bool_expr_tree<V>::evaluate(UnaryFunc&& var_evaluator, bool empty_val) const -> bool
    {
        if (m_nodes.empty())
        {
            return empty_val;
        }
        return evaluate_impl(var_evaluator, m_nodes.size() - 1);
    }

    template <typename V>
    template <typename UnaryFunc>
    auto flat_bool_expr_tree<V>::evaluate_impl(UnaryFunc& var_eval, std::size_t idx) const -> bool
    {
        assert(idx < m_nodes.size());
        return std::visit(
            [&](const auto& node) -> bool
            {
                using Node = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<Node, variable_type>)
                {
                    return var_eval(node);
                }
                else  // node == BoolOperator::logical_or
                {
                    // Note that some subtrees may not be explore thanks to short-circuiting
                    // of boolean && and ||
                    if ((node.op == BoolOperator::logical_and))
                    {
                        return evaluate_impl(var_eval, node.left_expr)
                               && evaluate_impl(var_eval, node.right_expr);
                    }
                    else  // node.op == BoolOperator::logical_or
                    {
                        return evaluate_impl(var_eval, node.left_expr)
                               || evaluate_impl(var_eval, node.right_expr);
                    }
                }
            },
            m_nodes[idx]
        );
    }

}
#endif
