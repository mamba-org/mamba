// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_GRAPH_UTIL_HPP
#define MAMBA_CORE_GRAPH_UTIL_HPP

#include <utility>
#include <vector>

namespace mamba
{
    // Simplified implementation of a directed graph
    template <typename Node>
    class DiGraph
    {
    public:
        using node_t = Node;
        using node_id = std::size_t;
        using node_list = std::vector<node_t>;
        using edge_list = std::vector<node_id>;
        using adjacency_list = std::vector<edge_list>;

        const edge_list& successors(node_id id) const;
        const edge_list& predecessors(node_id id) const;

        const node_list& get_node_list() const;

        node_id add_node(const node_t& value);
        node_id add_node(node_t&& value);
        void add_edge(node_id from, node_id to);

        template <class V>
        void depth_first_search(V& visitor, node_id start = node_id(0)) const;

    private:
        enum class color
        {
            white,
            gray,
            black
        };

        using color_list = std::vector<color>;

        template <class V>
        node_id add_node_impl(V&& value);

        template <class V>
        void depth_first_search_impl(V& visitor, node_id node, color_list& colors) const;

        node_list m_node_list;
        adjacency_list m_predecessors;
        adjacency_list m_successors;
    };

    template <class G>
    class default_visitor
    {
    public:
        using graph_type = G;
        using node_id = typename graph_type::node_id;

        void start_node(node_id, const G&)
        {
        }
        void finish_node(node_id, const G&)
        {
        }

        void start_edge(node_id, node_id, const G&)
        {
        }
        void tree_edge(node_id, node_id, const G&)
        {
        }
        void back_edge(node_id, node_id, const G&)
        {
        }
        void forward_or_cross_edge(node_id, node_id, const G&)
        {
        }
        void finish_edge(node_id, node_id, const G&)
        {
        }
    };

    /************************
     * graph implementation *
     ************************/

    template <class T>
    inline auto DiGraph<T>::get_node_list() const -> const node_list&
    {
        return m_node_list;
    }

    template <class T>
    inline auto DiGraph<T>::successors(node_id id) const -> const edge_list&
    {
        return m_successors[id];
    }

    template <class T>
    inline auto DiGraph<T>::predecessors(node_id id) const -> const edge_list&
    {
        return m_predecessors[id];
    }

    template <class T>
    inline auto DiGraph<T>::add_node(const node_t& value) -> node_id
    {
        return add_node_impl(value);
    }

    template <class T>
    inline auto DiGraph<T>::add_node(node_t&& value) -> node_id
    {
        return add_node_impl(std::move(value));
    }

    template <class T>
    inline void DiGraph<T>::add_edge(node_id from, node_id to)
    {
        m_successors[from].push_back(to);
        m_predecessors[to].push_back(from);
    }

    template <class T>
    template <class V>
    inline void DiGraph<T>::depth_first_search(V& visitor, node_id node) const
    {
        if (!m_node_list.empty())
        {
            color_list colors(m_node_list.size(), color::white);
            depth_first_search_impl(visitor, node, colors);
        }
    }

    template <class T>
    template <class V>
    inline auto DiGraph<T>::add_node_impl(V&& value) -> node_id
    {
        m_node_list.push_back(std::forward<V>(value));
        m_successors.push_back(edge_list());
        m_predecessors.push_back(edge_list());
        return m_node_list.size() - 1u;
    }

    template <class T>
    template <class V>
    inline void DiGraph<T>::depth_first_search_impl(V& visitor,
                                                    node_id node,
                                                    color_list& colors) const
    {
        colors[node] = color::gray;
        visitor.start_node(node, *this);
        for (auto child : m_successors[node])
        {
            visitor.start_edge(node, child, *this);
            if (colors[child] == color::white)
            {
                visitor.tree_edge(node, child, *this);
                depth_first_search_impl(visitor, child, colors);
            }
            else if (colors[child] == color::gray)
            {
                visitor.back_edge(node, child, *this);
            }
            else
            {
                visitor.forward_or_cross_edge(node, child, *this);
            }
            visitor.finish_edge(node, child, *this);
        }
        colors[node] = color::black;
        visitor.finish_node(node, *this);
    }

}  // namespace mamba

#endif  // INCLUDE_GRAPH_UTIL_HPP
