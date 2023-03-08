// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_GRAPH_UTIL_HPP
#define MAMBA_CORE_GRAPH_UTIL_HPP

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <utility>
#include <vector>

namespace mamba
{

    /**
     * A sorted vector behaving like a set.
     *
     * Like, ``std::set``, uniqueness is determined by using the equivalence relation.
     * In imprecise terms, two objects ``a`` and ``b`` are considered equivalent if neither
     * compares less than the other: ``!comp(a, b) && !comp(b, a)``
     */
    template <typename Key, typename Compare = std::less<Key>, typename Allocator = std::allocator<Key>>
    class vector_set : private std::vector<Key, Allocator>
    {
    public:

        using Base = std::vector<Key, Allocator>;
        using typename Base::allocator_type;
        using typename Base::const_iterator;
        using typename Base::const_reverse_iterator;
        using typename Base::size_type;
        using typename Base::value_type;
        using key_compare = Compare;
        using value_compare = Compare;

        using Base::cbegin;
        using Base::cend;
        using Base::crbegin;
        using Base::crend;

        using Base::clear;
        using Base::empty;
        using Base::reserve;
        using Base::size;

        vector_set() = default;
        vector_set(
            std::initializer_list<value_type> il,
            key_compare compare = key_compare(),
            const allocator_type& alloc = allocator_type()
        );
        template <typename InputIterator>
        vector_set(
            InputIterator first,
            InputIterator last,
            key_compare compare = key_compare(),
            const allocator_type& alloc = Allocator()
        );
        vector_set(const vector_set&) = default;
        vector_set(vector_set&&) = default;
        explicit vector_set(std::vector<Key, Allocator>&& other, key_compare compare = key_compare());
        explicit vector_set(const std::vector<Key, Allocator>& other, key_compare compare = key_compare());

        vector_set& operator=(const vector_set&) = default;
        vector_set& operator=(vector_set&&) = default;

        bool contains(const value_type&) const;
        const value_type& front() const noexcept;
        const value_type& back() const noexcept;

        const_iterator begin() const noexcept;
        const_iterator end() const noexcept;
        const_reverse_iterator rbegin() const noexcept;
        const_reverse_iterator rend() const noexcept;

        /** Insert an element in the set.
         *
         * Like std::vector and unlike std::set, inserting an element invalidates iterators.
         */
        std::pair<const_iterator, bool> insert(value_type&& value);
        std::pair<const_iterator, bool> insert(const value_type& value);
        template <typename InputIterator>
        void insert(InputIterator first, InputIterator last);

        const_iterator erase(const_iterator pos);
        const_iterator erase(const_iterator first, const_iterator last);
        size_type erase(const value_type& value);

    private:

        key_compare m_compare;

        bool key_eq(const value_type& a, const value_type& b) const;
        template <typename U>
        std::pair<const_iterator, bool> insert_impl(U&& value);
        void sort_and_remove_duplicates();

        template <typename K, typename C, typename A>
        friend bool operator==(const vector_set<K, C, A>& lhs, const vector_set<K, C, A>& rhs);
    };

    template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
    vector_set(std::initializer_list<Key>, Compare = Compare(), Allocator = Allocator())
        -> vector_set<Key, Compare, Allocator>;

    template <
        class InputIt,
        class Comp = std::less<typename std::iterator_traits<InputIt>::value_type>,
        class Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
    vector_set(InputIt, InputIt, Comp = Comp(), Alloc = Alloc())
        -> vector_set<typename std::iterator_traits<InputIt>::value_type, Comp, Alloc>;

    template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
    vector_set(std::vector<Key, Allocator>&&, Compare compare = Compare())
        -> vector_set<Key, Compare, Allocator>;

    template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
    vector_set(const std::vector<Key, Allocator>&, Compare compare = Compare())
        -> vector_set<Key, Compare, Allocator>;

    template <typename Key, typename Compare, typename Allocator>
    bool operator==(
        const vector_set<Key, Compare, Allocator>& lhs,
        const vector_set<Key, Compare, Allocator>& rhs
    );

    template <typename Key, typename Compare, typename Allocator>
    bool operator!=(
        const vector_set<Key, Compare, Allocator>& lhs,
        const vector_set<Key, Compare, Allocator>& rhs
    );

    // Simplified implementation of a directed graph
    template <typename Node, typename Derived>
    class DiGraphBase
    {
    public:

        using node_t = Node;
        using node_id = std::size_t;
        using node_map = std::map<node_id, node_t>;
        using node_id_list = vector_set<node_id>;
        using adjacency_list = std::vector<node_id_list>;

        node_id add_node(const node_t& value);
        node_id add_node(node_t&& value);
        bool add_edge(node_id from, node_id to);
        bool remove_edge(node_id from, node_id to);
        bool remove_node(node_id id);

        bool empty() const;
        std::size_t number_of_nodes() const noexcept;
        std::size_t number_of_edges() const noexcept;
        std::size_t in_degree(node_id id) const noexcept;
        std::size_t out_degree(node_id id) const noexcept;
        const node_map& nodes() const;
        const node_t& node(node_id id) const;
        node_t& node(node_id id);
        const node_id_list& successors(node_id id) const;
        const adjacency_list& successors() const;
        const node_id_list& predecessors(node_id id) const;
        const adjacency_list& predecessors() const;
        bool has_node(node_id id) const;
        bool has_edge(node_id from, node_id to) const;

        // TODO C++20 better to return a range since this search cannot be interupted from the
        // visitor
        template <typename UnaryFunc>
        UnaryFunc for_each_node_id(UnaryFunc func) const;
        template <typename BinaryFunc>
        BinaryFunc for_each_edge_id(BinaryFunc func) const;
        template <typename UnaryFunc>
        UnaryFunc for_each_leaf_id(UnaryFunc func) const;
        template <typename UnaryFunc>
        UnaryFunc for_each_leaf_id_from(node_id source, UnaryFunc func) const;
        template <typename UnaryFunc>
        UnaryFunc for_each_root_id(UnaryFunc func) const;
        template <typename UnaryFunc>
        UnaryFunc for_each_root_id_from(node_id source, UnaryFunc func) const;

        // TODO C++20 better to return a range since this search cannot be interupted from the
        // visitor
        template <class V>
        void depth_first_search(V& visitor, node_id start = node_id(0), bool reverse = false) const;

    protected:

        using derived_t = Derived;

        DiGraphBase() = default;
        DiGraphBase(const DiGraphBase&) = default;
        DiGraphBase(DiGraphBase&&) = default;
        DiGraphBase& operator=(const DiGraphBase&) = default;
        DiGraphBase& operator=(DiGraphBase&&) = default;
        ~DiGraphBase() = default;

        node_id number_of_node_id() const noexcept;

        Derived& derived_cast();
        const Derived& derived_cast() const;

    private:

        enum class visited
        {
            no,
            ongoing,
            yes
        };

        using visited_list = std::vector<visited>;

        template <class V>
        node_id add_node_impl(V&& value);

        template <class V>
        void depth_first_search_impl(
            V& visitor,
            node_id node,
            visited_list& status,
            const adjacency_list& successors
        ) const;

        // Source of truth for exsising nodes
        node_map m_node_map;
        // May contains empty slots after `remove_node`
        adjacency_list m_predecessors;
        // May contains empty slots after `remove_node`
        adjacency_list m_successors;
        std::size_t m_number_of_edges = 0;
    };

    template <typename Node, typename Derived>
    auto is_reachable(
        const DiGraphBase<Node, Derived>& graph,
        typename DiGraphBase<Node, Derived>::node_id source,
        typename DiGraphBase<Node, Derived>::node_id target
    ) -> bool;

    template <class G>
    class default_visitor
    {
    public:

        using graph_t = G;
        using node_id = typename graph_t::node_id;

        void start_node(node_id, const graph_t&)
        {
        }
        void finish_node(node_id, const graph_t&)
        {
        }

        void start_edge(node_id, node_id, const graph_t&)
        {
        }
        void tree_edge(node_id, node_id, const graph_t&)
        {
        }
        void back_edge(node_id, node_id, const graph_t&)
        {
        }
        void forward_or_cross_edge(node_id, node_id, const graph_t&)
        {
        }
        void finish_edge(node_id, node_id, const graph_t&)
        {
        }
    };

    template <typename Node, typename Edge = void>
    class DiGraph : private DiGraphBase<Node, DiGraph<Node, Edge>>
    {
    public:

        using Base = DiGraphBase<Node, DiGraph<Node, Edge>>;
        using typename Base::adjacency_list;
        using typename Base::node_id;
        using typename Base::node_id_list;
        using typename Base::node_map;
        using typename Base::node_t;
        using edge_t = Edge;
        using edge_id = std::pair<node_id, node_id>;
        using edge_map = std::map<edge_id, edge_t>;

        using Base::empty;
        using Base::has_edge;
        using Base::has_node;
        using Base::in_degree;
        using Base::node;
        using Base::nodes;
        using Base::number_of_edges;
        using Base::number_of_nodes;
        using Base::out_degree;
        using Base::predecessors;
        using Base::successors;

        using Base::for_each_edge_id;
        using Base::for_each_leaf_id;
        using Base::for_each_leaf_id_from;
        using Base::for_each_node_id;
        using Base::for_each_root_id;
        using Base::for_each_root_id_from;

        using Base::depth_first_search;

        using Base::add_node;
        bool add_edge(node_id from, node_id to, const edge_t& data);
        bool add_edge(node_id from, node_id to, edge_t&& data);
        bool remove_edge(node_id from, node_id to);
        bool remove_node(node_id id);

        const edge_map& edges() const;
        const edge_t& edge(node_id from, node_id to) const;
        const edge_t& edge(edge_id edge) const;
        edge_t& edge(node_id from, node_id to);
        edge_t& edge(edge_id edge);

    private:

        friend class DiGraphBase<Node, DiGraph<Node, Edge>>;  // required for private CRTP

        template <typename T>
        bool add_edge_impl(node_id from, node_id to, T&& data);

        edge_map m_edges;
    };

    template <typename Node>
    class DiGraph<Node, void> : public DiGraphBase<Node, DiGraph<Node, void>>
    {
    };

    /*******************************
     *  vector_set Implementation  *
     *******************************/

    template <typename K, typename C, typename A>
    vector_set<K, C, A>::vector_set(
        std::initializer_list<value_type> il,
        key_compare compare,
        const allocator_type& alloc
    )
        : Base(std::move(il), alloc)
        , m_compare(std::move(compare))
    {
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    template <typename InputIterator>
    vector_set<K, C, A>::vector_set(
        InputIterator first,
        InputIterator last,
        key_compare compare,
        const allocator_type& alloc
    )
        : Base(first, last, alloc)
        , m_compare(std::move(compare))
    {
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    vector_set<K, C, A>::vector_set(std::vector<K, A>&& other, C compare)
        : Base(std::move(other))
        , m_compare(std::move(compare))
    {
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    vector_set<K, C, A>::vector_set(const std::vector<K, A>& other, C compare)
        : Base(std::move(other))
        , m_compare(std::move(compare))
    {
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::contains(const value_type& value) const -> bool
    {
        return std::binary_search(begin(), end(), value);
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::front() const noexcept -> const value_type&
    {
        return Base::front();
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::back() const noexcept -> const value_type&
    {
        return Base::back();
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::begin() const noexcept -> const_iterator
    {
        return Base::begin();
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::end() const noexcept -> const_iterator
    {
        return Base::end();
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::rbegin() const noexcept -> const_reverse_iterator
    {
        return Base::rbegin();
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::rend() const noexcept -> const_reverse_iterator
    {
        return Base::rend();
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::insert(const value_type& value) -> std::pair<const_iterator, bool>
    {
        return insert_impl(value);
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::insert(value_type&& value) -> std::pair<const_iterator, bool>
    {
        return insert_impl(std::move(value));
    }

    template <typename K, typename C, typename A>
    bool vector_set<K, C, A>::key_eq(const value_type& a, const value_type& b) const
    {
        return !m_compare(a, b) && !m_compare(b, a);
    }

    template <typename K, typename C, typename A>
    void vector_set<K, C, A>::sort_and_remove_duplicates()
    {
        std::sort(Base::begin(), Base::end(), m_compare);
        auto is_eq = [this](const value_type& a, const value_type& b) { return key_eq(a, b); };
        Base::erase(std::unique(Base::begin(), Base::end(), is_eq), Base::end());
    }

    template <typename K, typename C, typename A>
    template <typename InputIterator>
    void vector_set<K, C, A>::insert(InputIterator first, InputIterator last)
    {
        Base::insert(Base::end(), first, last);
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    template <typename U>
    auto vector_set<K, C, A>::insert_impl(U&& value) -> std::pair<const_iterator, bool>
    {
        auto it = std::lower_bound(begin(), end(), value, m_compare);
        if ((it != end()) && (key_eq(*it, value)))
        {
            return { it, false };
        }
        it = Base::insert(it, std::forward<U>(value));
        return { it, true };
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::erase(const_iterator pos) -> const_iterator
    {
        // No need to sort or remove duplicates again
        return Base::erase(pos);
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::erase(const_iterator first, const_iterator last) -> const_iterator
    {
        // No need to sort or remove duplicates again
        return Base::erase(first, last);
    }

    template <typename K, typename C, typename A>
    auto vector_set<K, C, A>::erase(const value_type& value) -> size_type
    {
        auto it = std::lower_bound(begin(), end(), value, m_compare);
        if ((it == end()) || (!(key_eq(*it, value))))
        {
            return 0;
        }
        erase(it);
        return 1;
    }

    template <typename K, typename C, typename A>
    bool operator==(const vector_set<K, C, A>& lhs, const vector_set<K, C, A>& rhs)
    {
        auto is_eq = [&lhs](const auto& a, const auto& b) { return lhs.key_eq(a, b); };
        return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), is_eq);
    }

    template <typename K, typename C, typename A>
    bool operator!=(const vector_set<K, C, A>& lhs, const vector_set<K, C, A>& rhs)
    {
        return !(lhs == rhs);
    }

    /********************************
     *  DiGraphBase Implementation  *
     ********************************/

    template <typename N, typename G>
    bool DiGraphBase<N, G>::empty() const
    {
        return number_of_nodes() == 0;
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::number_of_nodes() const noexcept -> std::size_t
    {
        return m_node_map.size();
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::number_of_edges() const noexcept -> std::size_t
    {
        return m_number_of_edges;
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::in_degree(node_id id) const noexcept -> std::size_t
    {
        return m_predecessors[id].size();
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::out_degree(node_id id) const noexcept -> std::size_t
    {
        return m_successors[id].size();
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::nodes() const -> const node_map&
    {
        return m_node_map;
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::node(node_id id) const -> const node_t&
    {
        return m_node_map.at(id);
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::node(node_id id) -> node_t&
    {
        return m_node_map.at(id);
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::successors(node_id id) const -> const node_id_list&
    {
        return m_successors[id];
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::successors() const -> const adjacency_list&
    {
        return m_successors;
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::predecessors(node_id id) const -> const node_id_list&
    {
        return m_predecessors[id];
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::predecessors() const -> const adjacency_list&
    {
        return m_predecessors;
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::has_node(node_id id) const -> bool
    {
        return nodes().count(id) > 0;
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::has_edge(node_id from, node_id to) const -> bool
    {
        return has_node(from) && successors(from).contains(to);
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::add_node(const node_t& value) -> node_id
    {
        return add_node_impl(value);
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::add_node(node_t&& value) -> node_id
    {
        return add_node_impl(std::move(value));
    }

    template <typename N, typename G>
    template <class V>
    auto DiGraphBase<N, G>::add_node_impl(V&& value) -> node_id
    {
        const node_id id = number_of_node_id();
        m_node_map.emplace(id, std::forward<V>(value));
        m_successors.push_back(node_id_list());
        m_predecessors.push_back(node_id_list());
        return id;
    }

    template <typename N, typename G>
    bool DiGraphBase<N, G>::remove_node(node_id id)
    {
        if (!has_node(id))
        {
            return false;
        }

        const auto succs = successors(id);  // Cannot iterate on object being modified
        for (const auto& to : succs)
        {
            remove_edge(id, to);
        }
        const auto preds = predecessors(id);  // Cannot iterate on object being modified
        for (const auto& from : preds)
        {
            remove_edge(from, id);
        }
        m_node_map.erase(id);

        return true;
    }

    template <typename N, typename G>
    bool DiGraphBase<N, G>::add_edge(node_id from, node_id to)
    {
        if (has_edge(from, to))
        {
            return false;
        }
        m_successors[from].insert(to);
        m_predecessors[to].insert(from);
        ++m_number_of_edges;
        return true;
    }

    template <typename N, typename G>
    bool DiGraphBase<N, G>::remove_edge(node_id from, node_id to)
    {
        if (!has_edge(from, to))
        {
            return false;
        }
        m_successors[from].erase(to);
        m_predecessors[to].erase(from);
        --m_number_of_edges;
        return true;
    }

    template <typename N, typename G>
    template <typename UnaryFunc>
    UnaryFunc DiGraphBase<N, G>::for_each_node_id(UnaryFunc func) const
    {
        for (const auto& [i, _] : m_node_map)
        {
            func(i);
        }
        return func;
    }

    template <typename N, typename G>
    template <typename BinaryFunc>
    BinaryFunc DiGraphBase<N, G>::for_each_edge_id(BinaryFunc func) const
    {
        for_each_node_id(
            [&](node_id i)
            {
                for (node_id j : successors(i))
                {
                    func(i, j);
                }
            }
        );
        return func;
    }

    template <typename N, typename G>
    template <typename UnaryFunc>
    UnaryFunc DiGraphBase<N, G>::for_each_leaf_id(UnaryFunc func) const
    {
        for_each_node_id(
            [&](node_id i)
            {
                if (out_degree(i) == 0)
                {
                    func(i);
                }
            }
        );
        return func;
    }

    template <typename N, typename G>
    template <typename UnaryFunc>
    UnaryFunc DiGraphBase<N, G>::for_each_root_id(UnaryFunc func) const
    {
        for_each_node_id(
            [&](node_id i)
            {
                if (in_degree(i) == 0)
                {
                    func(i);
                }
            }
        );
        return func;
    }

    template <typename N, typename G>
    template <typename UnaryFunc>
    UnaryFunc DiGraphBase<N, G>::for_each_leaf_id_from(node_id source, UnaryFunc func) const
    {
        struct LeafVisitor : default_visitor<derived_t>
        {
            UnaryFunc& m_func;

            LeafVisitor(UnaryFunc& func)
                : m_func{ func }
            {
            }

            void start_node(node_id n, const derived_t& g)
            {
                if (g.out_degree(n) == 0)
                {
                    m_func(n);
                }
            }
        };
        auto visitor = LeafVisitor(func);
        depth_first_search(visitor, source);
        return func;
    }

    template <typename N, typename G>
    template <typename UnaryFunc>
    UnaryFunc DiGraphBase<N, G>::for_each_root_id_from(node_id source, UnaryFunc func) const
    {
        struct RootVisitor : default_visitor<derived_t>
        {
            UnaryFunc& m_func;

            RootVisitor(UnaryFunc& func)
                : m_func{ func }
            {
            }

            void start_node(node_id n, const derived_t& g)
            {
                if (g.in_degree(n) == 0)
                {
                    m_func(n);
                }
            }
        };
        auto visitor = RootVisitor(func);
        depth_first_search(visitor, source, /*reverse*/ true);
        return func;
    }

    template <typename N, typename G>
    template <class V>
    void DiGraphBase<N, G>::depth_first_search(V& visitor, node_id node, bool reverse) const
    {
        if (!empty())
        {
            visited_list status(number_of_node_id(), visited::no);
            depth_first_search_impl(visitor, node, status, reverse ? m_predecessors : m_successors);
        }
    }

    template <typename N, typename G>
    template <class V>
    void DiGraphBase<N, G>::depth_first_search_impl(
        V& visitor,
        node_id node,
        visited_list& status,
        const adjacency_list& successors
    ) const
    {
        status[node] = visited::ongoing;
        visitor.start_node(node, derived_cast());
        for (auto child : successors[node])
        {
            visitor.start_edge(node, child, derived_cast());
            if (status[child] == visited::no)
            {
                visitor.tree_edge(node, child, derived_cast());
                depth_first_search_impl(visitor, child, status, successors);
            }
            else if (status[child] == visited::ongoing)
            {
                visitor.back_edge(node, child, derived_cast());
            }
            else
            {
                visitor.forward_or_cross_edge(node, child, derived_cast());
            }
            visitor.finish_edge(node, child, derived_cast());
        }
        status[node] = visited::yes;
        visitor.finish_node(node, derived_cast());
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::number_of_node_id() const noexcept -> node_id
    {
        // Not number_of_nodes because due to remove nodes it may be larger
        return m_successors.size();
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::derived_cast() -> derived_t&
    {
        return static_cast<derived_t&>(*this);
    }

    template <typename N, typename G>
    auto DiGraphBase<N, G>::derived_cast() const -> const derived_t&
    {
        return static_cast<const derived_t&>(*this);
    }

    /*******************************
     *  Algorithms implementation  *
     *******************************/

    template <typename Graph>
    auto
    is_reachable(const Graph& graph, typename Graph::node_id source, typename Graph::node_id target)
        -> bool
    {
        struct : default_visitor<Graph>
        {
            using node_id = typename Graph::node_id;
            node_id target;
            bool target_visited = false;

            void start_node(node_id node, const Graph&)
            {
                target_visited = target_visited || (node == target);
            }
        } visitor{ {}, target };

        graph.depth_first_search(visitor, source);
        return visitor.target_visited;
    }

    /*********************************
     *  DiGraph Edge Implementation  *
     *********************************/

    template <typename N, typename E>
    bool DiGraph<N, E>::add_edge(node_id from, node_id to, const edge_t& data)
    {
        return add_edge_impl(from, to, data);
    }

    template <typename N, typename E>
    bool DiGraph<N, E>::add_edge(node_id from, node_id to, edge_t&& data)
    {
        return add_edge_impl(from, to, std::move(data));
    }

    template <typename N, typename E>
    template <typename T>
    bool DiGraph<N, E>::add_edge_impl(node_id from, node_id to, T&& data)
    {
        if (const bool added = Base::add_edge(from, to); added)
        {
            auto l_edge_id = std::pair(from, to);
            m_edges.insert(std::pair(l_edge_id, std::forward<T>(data)));
            return true;
        }
        return false;
    }

    template <typename N, typename E>
    bool DiGraph<N, E>::remove_edge(node_id from, node_id to)
    {
        m_edges.erase({ from, to });  // No-op if edge does not exists
        return Base::remove_edge(from, to);
    }

    template <typename N, typename E>
    bool DiGraph<N, E>::remove_node(node_id id)
    {
        // No-op if edge does not exists
        for (const auto& to : successors(id))
        {
            m_edges.erase({ id, to });
        }
        for (const auto& from : predecessors(id))
        {
            m_edges.erase({ from, id });
        }
        return Base::remove_node(id);
    }

    template <typename N, typename E>
    auto DiGraph<N, E>::edges() const -> const edge_map&
    {
        return m_edges;
    }

    template <typename N, typename E>
    auto DiGraph<N, E>::edge(edge_id edge) const -> const edge_t&
    {
        return m_edges.at(edge);
    }

    template <typename N, typename E>
    auto DiGraph<N, E>::edge(node_id from, node_id to) const -> const edge_t&
    {
        return edge({ from, to });
    }

    template <typename N, typename E>
    auto DiGraph<N, E>::edge(edge_id edge) -> edge_t&
    {
        return m_edges[edge];
    }

    template <typename N, typename E>
    auto DiGraph<N, E>::edge(node_id from, node_id to) -> edge_t&
    {
        return edge({ from, to });
    }

}  // namespace mamba

#endif  // INCLUDE_GRAPH_UTIL_HPP
