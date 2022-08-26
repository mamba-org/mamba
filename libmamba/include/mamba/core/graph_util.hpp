// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_GRAPH_UTIL_HPP
#define MAMBA_CORE_GRAPH_UTIL_HPP

#include <utility>
#include <vector>
#include <map>
#include <set>

namespace mamba
{
    // Simplified implementation of a directed graph
    template <typename Node, typename Derived>
    class DiGraphBase
    {
    public:
        using node_t = Node;
        using node_id = std::size_t;
        using node_list = std::vector<node_t>;
        using node_id_list = std::set<node_id>;
        using adjacency_list = std::vector<node_id_list>;

        node_id add_node(const node_t& value);
        node_id add_node(node_t&& value);
        void add_edge(node_id from, node_id to);

        std::size_t number_of_nodes() const;
        const node_list& nodes() const;
        const node_id_list& successors(node_id id) const;
        const node_id_list& predecessors(node_id id) const;

        template <typename UnaryFunc>
        UnaryFunc for_each_leaf(UnaryFunc func) const;
        template <typename UnaryFunc>
        UnaryFunc for_each_root(UnaryFunc func) const;

        template <class V>
        void depth_first_search(V& visitor, node_id start = node_id(0)) const;

    protected:
        DiGraphBase() = default;
        DiGraphBase(DiGraphBase const&) = default;
        DiGraphBase(DiGraphBase&&) = default;
        DiGraphBase& operator=(DiGraphBase const&) = default;
        DiGraphBase& operator=(DiGraphBase&&) = default;
        ~DiGraphBase() = default;

        Derived* derived_cast()
        {
            return static_cast<Derived*>(this);
        }
        Derived const* derived_cast() const
        {
            return static_cast<Derived const*>(this);
        }

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
        void depth_first_search_impl(V& visitor, node_id node, visited_list& status) const;

        node_list m_node_list;
        adjacency_list m_predecessors;
        adjacency_list m_successors;
    };

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
    class DiGraph : public DiGraphBase<Node, DiGraph<Node, Edge>>
    {
    public:
        using Base = DiGraphBase<Node, DiGraph<Node, Edge>>;
        using edge_t = Edge;
        using node_id = typename Base::node_id;
        using edge_id = std::pair<node_id, node_id>;
        using edge_map = std::map<edge_id, edge_t>;

        void add_edge(node_id from, node_id to, edge_t const& data);
        void add_edge(node_id from, node_id to, edge_t&& data);

        const edge_map& edges() const;

    private:
        template <typename T>
        void add_edge_impl(node_id from, node_id to, T&& data);

        edge_map m_edges;
    };

    template <typename Node>
    class DiGraph<Node, void> : public DiGraphBase<Node, DiGraph<Node, void>>
    {
    };

    /********************************
     *  DiGraphBase Implementation  *
     ********************************/

    template <typename N, typename G>
    inline auto DiGraphBase<N, G>::number_of_nodes() const -> std::size_t
    {
        return m_node_list.size();
    }

    template <typename N, typename G>
    inline auto DiGraphBase<N, G>::nodes() const -> const node_list&
    {
        return m_node_list;
    }

    template <typename N, typename G>
    inline auto DiGraphBase<N, G>::successors(node_id id) const -> const node_id_list&
    {
        return m_successors[id];
    }

    template <typename N, typename G>
    inline auto DiGraphBase<N, G>::predecessors(node_id id) const -> const node_id_list&
    {
        return m_predecessors[id];
    }

    template <typename N, typename G>
    inline auto DiGraphBase<N, G>::add_node(const node_t& value) -> node_id
    {
        return add_node_impl(value);
    }

    template <typename N, typename G>
    inline auto DiGraphBase<N, G>::add_node(node_t&& value) -> node_id
    {
        return add_node_impl(std::move(value));
    }

    template <typename N, typename G>
    inline void DiGraphBase<N, G>::add_edge(node_id from, node_id to)
    {
        m_successors[from].insert(to);
        m_predecessors[to].insert(from);
    }

    template <typename N, typename G>
    template <typename UnaryFunc>
    inline UnaryFunc DiGraphBase<N, G>::for_each_leaf(UnaryFunc func) const
    {
        auto const n_nodes = number_of_nodes();
        for (node_id i = 0; i < n_nodes; ++i)
        {
            if (m_successors[i].size() == 0)
            {
                func(i);
            }
        }
        return func;
    }

    template <typename N, typename G>
    template <typename UnaryFunc>
    inline UnaryFunc DiGraphBase<N, G>::for_each_root(UnaryFunc func) const
    {
        auto const n_nodes = number_of_nodes();
        for (node_id i = 0; i < n_nodes; ++i)
        {
            if (m_predecessors[i].size() == 0)
            {
                func(i);
            }
        }
        return func;
    }

    template <typename N, typename G>
    template <class V>
    inline void DiGraphBase<N, G>::depth_first_search(V& visitor, node_id node) const
    {
        if (!m_node_list.empty())
        {
            visited_list status(m_node_list.size(), visited::no);
            depth_first_search_impl(visitor, node, status);
        }
    }

    template <typename N, typename G>
    template <class V>
    inline auto DiGraphBase<N, G>::add_node_impl(V&& value) -> node_id
    {
        m_node_list.push_back(std::forward<V>(value));
        m_successors.push_back(node_id_list());
        m_predecessors.push_back(node_id_list());
        return m_node_list.size() - 1u;
    }

    template <typename N, typename G>
    template <class V>
    inline void DiGraphBase<N, G>::depth_first_search_impl(V& visitor,
                                                           node_id node,
                                                           visited_list& status) const
    {
        status[node] = visited::ongoing;
        visitor.start_node(node, *derived_cast());
        for (auto child : m_successors[node])
        {
            visitor.start_edge(node, child, *derived_cast());
            if (status[child] == visited::no)
            {
                visitor.tree_edge(node, child, *derived_cast());
                depth_first_search_impl(visitor, child, status);
            }
            else if (status[child] == visited::ongoing)
            {
                visitor.back_edge(node, child, *derived_cast());
            }
            else
            {
                visitor.forward_or_cross_edge(node, child, *derived_cast());
            }
            visitor.finish_edge(node, child, *derived_cast());
        }
        status[node] = visited::yes;
        visitor.finish_node(node, *derived_cast());
    }

    /*********************************
     *  DiGraph Edge Implementation  *
     *********************************/

    template <typename N, typename E>
    inline void DiGraph<N, E>::add_edge(node_id from, node_id to, edge_t const& data)
    {
        add_edge_impl(from, to, data);
    }

    template <typename N, typename E>
    inline void DiGraph<N, E>::add_edge(node_id from, node_id to, edge_t&& data)
    {
        add_edge_impl(from, to, std::move(data));
    }

    template <typename N, typename E>
    template <typename T>
    inline void DiGraph<N, E>::add_edge_impl(node_id from, node_id to, T&& data)
    {
        Base::add_edge(from, to);
        m_edges[{ from, to }] = std::forward<T>(data);
    }

    template <typename N, typename E>
    inline auto DiGraph<N, E>::edges() const -> const edge_map&
    {
        return m_edges;
    }

}  // namespace mamba

#endif  // INCLUDE_GRAPH_UTIL_HPP
