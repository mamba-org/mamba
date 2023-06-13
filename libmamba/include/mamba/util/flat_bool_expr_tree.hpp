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
    /**
     * In array binary tree.
     *
     * A binary tree, each node is either a leaf, or a node with exactly two children.
     * This data structure is light and nothing prevents the user from representing
     * any kind of binary directed acyclic graph (e.g. there can be multiple trees,
     * or nodes could have multiple parents).
     */
    template <typename Branch, typename Leaf>
    class flat_binary_tree
    {
    public:

        using branch_type = Branch;
        using leaf_type = Leaf;
        struct BranchNode
        {
            branch_type data;
            std::size_t left_child = 0;
            std::size_t right_child = 0;
        };
        using branch_node = BranchNode;
        using leaf_node = leaf_type;
        using node_type = std::variant<branch_node, leaf_node>;
        using node_list = std::vector<node_type>;
        using size_type = typename node_list::size_type;
        using idx_type = size_type;

        [[nodiscard]] auto size() const -> size_type;
        [[nodiscard]] auto empty() const -> bool;

        void clear();
        void reserve(size_type size);

        auto add_leaf(const leaf_type& leaf) -> idx_type;
        auto add_leaf(leaf_type&& leaf) -> idx_type;
        auto add_branch(const branch_type& branch, idx_type left_child, idx_type right_child)
            -> idx_type;
        auto add_branch(branch_type&& branch, idx_type left_child, idx_type right_child) -> idx_type;

        [[nodiscard]] auto node(idx_type idx) const -> const node_type&;
        [[nodiscard]] auto node(idx_type idx) -> node_type&;
        [[nodiscard]] auto is_branch(idx_type idx) const -> bool;
        [[nodiscard]] auto is_leaf(idx_type idx) const -> bool;
        [[nodiscard]] auto leaf(idx_type idx) const -> const leaf_type&;
        [[nodiscard]] auto leaf(idx_type idx) -> leaf_type&;
        [[nodiscard]] auto branch(idx_type idx) const -> const branch_type&;
        [[nodiscard]] auto branch(idx_type idx) -> branch_type&;
        [[nodiscard]] auto left(idx_type idx) const -> idx_type;
        [[nodiscard]] auto right(idx_type idx) const -> idx_type;
        [[nodiscard]] auto root() const -> idx_type;

    private:

        node_list m_nodes;
        idx_type m_root = 0;

        template <typename L>
        auto add_leaf_impl(L&& leaf) -> idx_type;
        template <typename B>
        auto add_branch_impl(B&& branch, idx_type left_child, idx_type right_child) -> idx_type;
    };

    template <typename Variable, typename Operator>
    class PostfixParser
    {
    public:

        using operator_type = Operator;
        using variable_type = Variable;
        using tree_type = flat_binary_tree<operator_type, variable_type>;

        void push_variable(const variable_type& var);
        void push_variable(variable_type&& var);
        void push_operator(const operator_type& op);
        void push_operator(operator_type&& op);

        [[nodiscard]] auto tree() const& -> const tree_type&;
        [[nodiscard]] auto tree() && -> tree_type&&;

    private:

        using idx_type = typename tree_type::idx_type;

        tree_type m_tree = {};
        idx_type m_left_end = 0;
        idx_type m_right_end = 0;

        auto left_start() const -> idx_type;
        auto right_start() const -> idx_type;
        auto next_left_end() const -> idx_type;

        template <typename V>
        void push_variable_impl(V&& var);
        template <typename O>
        void push_operator_impl(O&& op);
    };

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
    /****************************************
     *  Implementation of flat_binary_tree  *
     ****************************************/

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::size() const -> size_type
    {
        return m_nodes.size();
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::empty() const -> bool
    {
        return m_nodes.empty();
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::node(idx_type idx) const -> const node_type&
    {
        return m_nodes.at(idx);
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::node(idx_type idx) -> node_type&
    {
        return m_nodes.at(idx);
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::is_branch(idx_type idx) const -> bool
    {
        return std::holds_alternative<branch_node>(node(idx));
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::is_leaf(idx_type idx) const -> bool
    {
        return std::holds_alternative<leaf_node>(node(idx));
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::leaf(idx_type idx) const -> const leaf_type&
    {
        return std::get<leaf_node>(node(idx));
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::leaf(idx_type idx) -> leaf_type&
    {
        return std::get<leaf_node>(node(idx));
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::branch(idx_type idx) const -> const branch_type&
    {
        return std::get<branch_node>(node(idx)).data;
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::branch(idx_type idx) -> branch_type&
    {
        return std::get<branch_node>(node(idx)).data;
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::left(idx_type idx) const -> idx_type
    {
        return std::get<branch_node>(node(idx)).left_child;
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::right(idx_type idx) const -> idx_type
    {
        return std::get<branch_node>(node(idx)).right_child;
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::root() const -> idx_type
    {
        return m_root;
    }

    template <typename B, typename L>
    void flat_binary_tree<B, L>::clear()
    {
        return m_nodes.clear();
    }

    template <typename B, typename L>
    void flat_binary_tree<B, L>::reserve(size_type size)
    {
        return m_nodes.reserve(size);
    }

    template <typename B, typename L>
    template <typename Leaf>
    auto flat_binary_tree<B, L>::add_leaf_impl(Leaf&& leaf) -> idx_type
    {
        m_nodes.emplace_back(std::forward<Leaf>(leaf));
        return size() - 1;
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::add_leaf(const leaf_type& leaf) -> idx_type
    {
        return add_leaf_impl(leaf);
    }

    template <typename B, typename L>
    auto flat_binary_tree<B, L>::add_leaf(leaf_type&& leaf) -> idx_type
    {
        return add_leaf_impl(std::move(leaf));
    }

    template <typename B, typename L>
    template <typename Branch>
    auto
    flat_binary_tree<B, L>::add_branch_impl(Branch&& branch, idx_type left_child, idx_type right_child)
        -> idx_type
    {
        m_nodes.emplace_back(branch_node{ std::forward<Branch>(branch), left_child, right_child });
        const auto idx = size() - 1;
        if ((left_child == root()) || right_child == root())
        {
            m_root = idx;
        }
        return idx;
    }

    template <typename B, typename L>
    auto
    flat_binary_tree<B, L>::add_branch(const branch_type& branch, idx_type left_child, idx_type right_child)
        -> idx_type
    {
        return add_branch_impl(branch, left_child, right_child);
    }

    template <typename B, typename L>
    auto
    flat_binary_tree<B, L>::add_branch(branch_type&& branch, idx_type left_child, idx_type right_child)
        -> idx_type
    {
        return add_branch_impl(std::move(branch), left_child, right_child);
    }

    /*************************************
     *  Implementation of PostfixParser  *
     *************************************/

    template <typename V, typename O>
    auto PostfixParser<V, O>::left_start() const -> idx_type
    {
        assert(m_right_end >= 1);
        return m_right_end - 1;
    }

    template <typename V, typename O>
    auto PostfixParser<V, O>::right_start() const -> idx_type
    {
        assert(m_tree.size() >= 1);
        return m_tree.size() - 1;
    }

    template <typename V, typename O>
    auto PostfixParser<V, O>::next_left_end() const -> idx_type
    {
        if (m_left_end == 0)
        {
            return 0;
        }
        idx_type new_left_end = m_left_end - 1;
        while (m_tree.is_branch(new_left_end))
        {
            new_left_end = m_tree.left(new_left_end);
        }
        return new_left_end;
    }

    template <typename V, typename O>
    template <typename Var>
    void PostfixParser<V, O>::push_variable_impl(Var&& var)
    {
        m_left_end = std::exchange(m_right_end, m_tree.add_leaf(std::forward<Var>(var)));
    }

    template <typename V, typename O>
    void PostfixParser<V, O>::push_variable(const variable_type& var)
    {
        return push_variable_impl(var);
    }

    template <typename V, typename O>
    void PostfixParser<V, O>::push_variable(variable_type&& var)
    {
        return push_variable_impl(std::move(var));
    }

    template <typename V, typename O>
    template <typename Op>
    void PostfixParser<V, O>::push_operator_impl(Op&& op)
    {
        m_tree.add_branch(std::forward<Op>(op), left_start(), right_start());
        m_right_end = std::exchange(m_left_end, next_left_end());
    }

    template <typename V, typename O>
    void PostfixParser<V, O>::push_operator(const operator_type& op)
    {
        return push_operator_impl(op);
    }

    template <typename V, typename O>
    void PostfixParser<V, O>::push_operator(operator_type&& op)
    {
        return push_operator_impl(std::move(op));
    }

    template <typename V, typename O>
    auto PostfixParser<V, O>::tree() const& -> const tree_type&
    {
        return m_tree;
    }

    template <typename V, typename O>
    auto PostfixParser<V, O>::tree() && -> tree_type&&
    {
        return std::move(m_tree);
    }

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
