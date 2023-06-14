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
        void finalize();

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
        auto accept_operator() const -> bool;
    };

    template <typename Variable, typename Operator, typename OperatorCmp = std::less<>>
    class InfixParser
    {
    public:

        using operator_type = Operator;
        using variable_type = Variable;
        using tree_type = flat_binary_tree<operator_type, variable_type>;
        using operator_precedence_type = OperatorCmp;

        InfixParser(const operator_precedence_type& cmp);
        InfixParser(operator_precedence_type&& cmp = {});

        void push_variable(const variable_type& var);
        void push_variable(variable_type&& var);
        void push_operator(const operator_type& op);
        void push_operator(operator_type&& op);
        void push_left_parenthesis();
        void push_right_parenthesis();
        void finalize();

        [[nodiscard]] auto tree() const& -> const tree_type&;
        [[nodiscard]] auto tree() && -> tree_type&&;

    private:

        using postfix_parser_type = PostfixParser<variable_type, operator_type>;
        struct LeftParenthesis
        {
        };
        using operator_or_parenthesis_type = std::variant<operator_type, LeftParenthesis>;
        using operator_stack_type = std::vector<operator_or_parenthesis_type>;

        postfix_parser_type m_postfix_parser = {};
        operator_stack_type m_op_stack = {};
        operator_precedence_type m_op_cmp = {};

        template <typename T>
        void stack_push(T&& elem);
        auto stack_pop() -> operator_or_parenthesis_type;
        auto stack_empty() const -> bool;
        auto stack_top() const -> const operator_or_parenthesis_type&;
        auto stack_top_is_parenthesis() const -> bool;
        auto stack_top_is_op_with_greater_precedence_than(const operator_type&) const -> bool;

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
        using tree_type = flat_binary_tree<operator_type, variable_type>;
        using size_type = typename tree_type::size_type;

        template <typename Iter>
        [[nodiscard]] static auto from_postfix(Iter first, Iter last) -> self_type;

        flat_bool_expr_tree() = default;
        flat_bool_expr_tree(const tree_type& tree);
        flat_bool_expr_tree(tree_type&& tree);

        [[nodiscard]] auto size() const -> size_type;
        [[nodiscard]] auto empty() const -> bool;

        void clear();
        void reserve(size_type size);

        template <typename UnaryFunc = identity>
        [[nodiscard]] auto evaluate(UnaryFunc&& var_evaluator = {}, bool empty_val = true) const
            -> bool;

    private:

        using idx_type = typename tree_type::idx_type;

        template <typename UnaryFunc>
        auto evaluate_impl(UnaryFunc& var_evaluator, idx_type idx) const -> bool;

        tree_type m_tree = {};
    };
}

#include <cassert>
#include <iterator>
#include <stdexcept>
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
    auto PostfixParser<V, O>::accept_operator() const -> bool
    {
        return (m_tree.size() >= 2) && (m_left_end != m_right_end);
    }

    template <typename V, typename O>
    template <typename Op>
    void PostfixParser<V, O>::push_operator_impl(Op&& op)
    {
        if (!accept_operator())
        {
            throw std::invalid_argument("Invalid expression");
        }
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
    void PostfixParser<V, O>::finalize()
    {
        if ((!m_tree.empty()) && (m_tree.root() != (m_tree.size() - 1)))
        {
            throw std::invalid_argument("Incomplete expression");
        }
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

    /***********************************
     *  Implementation of InfixParser  *
     ***********************************/

    template <typename V, typename O, typename C>
    InfixParser<V, O, C>::InfixParser(const operator_precedence_type& cmp)
        : m_op_cmp(cmp)
    {
    }

    template <typename V, typename O, typename C>
    InfixParser<V, O, C>::InfixParser(operator_precedence_type&& cmp)
        : m_op_cmp(std::move(cmp))
    {
    }

    template <typename V, typename O, typename C>
    template <typename T>
    void InfixParser<V, O, C>::stack_push(T&& elem)
    {
        m_op_stack.push_back(std::forward<T>(elem));
    }

    template <typename V, typename O, typename C>
    auto InfixParser<V, O, C>::stack_pop() -> operator_or_parenthesis_type
    {
        assert(!stack_empty());
        auto top = stack_top();
        m_op_stack.pop_back();
        return top;
    }

    template <typename V, typename O, typename C>
    auto InfixParser<V, O, C>::stack_empty() const -> bool
    {
        return m_op_stack.empty();
    }

    template <typename V, typename O, typename C>
    auto InfixParser<V, O, C>::stack_top() const -> const operator_or_parenthesis_type&
    {
        assert(!stack_empty());
        return m_op_stack.back();
    }

    template <typename V, typename O, typename C>
    auto InfixParser<V, O, C>::stack_top_is_parenthesis() const -> bool
    {
        return (!stack_empty()) && std::holds_alternative<LeftParenthesis>(stack_top());
    }

    template <typename V, typename O, typename C>
    auto InfixParser<V, O, C>::stack_top_is_op_with_greater_precedence_than(const operator_type& op
    ) const -> bool
    {
        if (stack_empty())
        {
            return false;
        }
        if (const auto* const op_ptr = std::get_if<operator_type>(&stack_top()))
        {
            return m_op_cmp(op, *op_ptr);
        }
        return false;
    }

    template <typename V, typename O, typename C>
    template <typename Var>
    void InfixParser<V, O, C>::push_variable_impl(Var&& var)
    {
        m_postfix_parser.push_variable(std::forward<Var>(var));
    }

    template <typename V, typename O, typename C>
    void InfixParser<V, O, C>::push_variable(const variable_type& var)
    {
        return push_variable_impl(var);
    }

    template <typename V, typename O, typename C>
    void InfixParser<V, O, C>::push_variable(variable_type&& var)
    {
        return push_variable_impl(std::move(var));
    }

    template <typename V, typename O, typename C>
    template <typename Op>
    void InfixParser<V, O, C>::push_operator_impl(Op&& op)
    {
        while (stack_top_is_op_with_greater_precedence_than(op))
        {
            m_postfix_parser.push_operator(std::get<operator_type>(stack_pop()));
        }
        stack_push(std::forward<Op>(op));
    }

    template <typename V, typename O, typename C>
    void InfixParser<V, O, C>::push_operator(const operator_type& op)
    {
        return push_operator_impl(op);
    }

    template <typename V, typename O, typename C>
    void InfixParser<V, O, C>::push_operator(operator_type&& op)
    {
        return push_operator_impl(std::move(op));
    }

    template <typename V, typename O, typename C>
    void InfixParser<V, O, C>::push_left_parenthesis()
    {
        stack_push(LeftParenthesis{});
    }

    template <typename V, typename O, typename C>
    void InfixParser<V, O, C>::push_right_parenthesis()
    {
        while (!stack_top_is_parenthesis())
        {
            if (stack_empty())
            {
                throw std::invalid_argument("Mistmatched parenthesis");
            }
            m_postfix_parser.push_operator(std::get<operator_type>(stack_pop()));
        }
        assert(stack_top_is_parenthesis());
        stack_pop();
    }

    template <typename V, typename O, typename C>
    void InfixParser<V, O, C>::finalize()
    {
        while (!stack_empty())
        {
            if (stack_top_is_parenthesis())
            {
                throw std::invalid_argument("Mistmatched parenthesis");
            }
            m_postfix_parser.push_operator(std::get<operator_type>(stack_pop()));
        }
        m_postfix_parser.finalize();
    }

    template <typename V, typename O, typename C>
    auto InfixParser<V, O, C>::tree() const& -> const tree_type&
    {
        return m_postfix_parser.tree();
    }

    template <typename V, typename O, typename C>
    auto InfixParser<V, O, C>::tree() && -> tree_type&&
    {
        return std::move(m_postfix_parser).tree();
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
    flat_bool_expr_tree<V>::flat_bool_expr_tree(const tree_type& tree)
        : m_tree(tree)
    {
    }

    template <typename V>
    flat_bool_expr_tree<V>::flat_bool_expr_tree(tree_type&& tree)
        : m_tree(std::move(tree))
    {
    }

    template <typename V>
    auto flat_bool_expr_tree<V>::size() const -> size_type
    {
        return m_tree.size();
    }

    template <typename V>
    auto flat_bool_expr_tree<V>::empty() const -> bool
    {
        return m_tree.empty();
    }

    template <typename V>
    void flat_bool_expr_tree<V>::clear()
    {
        return m_tree.clear();
    }

    template <typename V>
    void flat_bool_expr_tree<V>::reserve(size_type size)
    {
        return m_tree.reserve(size);
    }

    template <typename V>
    template <typename UnaryFunc>
    auto flat_bool_expr_tree<V>::evaluate(UnaryFunc&& var_evaluator, bool empty_val) const -> bool
    {
        if (m_tree.empty())
        {
            return empty_val;
        }
        return evaluate_impl(var_evaluator, m_tree.root());
    }

    template <typename V>
    template <typename UnaryFunc>
    auto flat_bool_expr_tree<V>::evaluate_impl(UnaryFunc& var_eval, idx_type idx) const -> bool
    {
        // We do a tree evluation rather than a stack-based postfix evaluation to
        // avoid evaluation sub trees thanks to operator && and || short circuiting.
        assert(idx < m_tree.size());
        if (m_tree.is_leaf(idx))
        {
            return var_eval(m_tree.leaf(idx));
        }
        if ((m_tree.branch(idx) == BoolOperator::logical_and))
        {
            return evaluate_impl(var_eval, m_tree.left(idx))
                   && evaluate_impl(var_eval, m_tree.right(idx));
        }
        else  // BoolOperator::logical_or
        {
            return evaluate_impl(var_eval, m_tree.left(idx))
                   || evaluate_impl(var_eval, m_tree.right(idx));
        }
    }
}
#endif
