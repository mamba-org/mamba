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

namespace mamba
{
    // Simplified implementation of a directed graph
    template <typename Node, typename GraphCRTP>
    class DiGraphBase
    {
    public:
        using node_t = Node;
        using node_id = std::size_t;
        using node_list = std::vector<node_t>;
        using node_id_list = std::vector<node_id>;
        using adjacency_list = std::vector<node_id_list>;

        std::size_t number_of_nodes() const;

        const node_id_list& successors(node_id id) const;
        const node_id_list& predecessors(node_id id) const;

        const node_list& nodes() const;

        node_id add_node(const node_t& value);
        node_id add_node(node_t&& value);
        void add_edge(node_id from, node_id to);

        template <class V>
        void depth_first_search(V& visitor, node_id start = node_id(0)) const;

    protected:
        GraphCRTP* crtp_cast()
        {
            return static_cast<GraphCRTP*>(this);
        }
        GraphCRTP const* crtp_cast() const
        {
            return static_cast<GraphCRTP const*>(this);
        }

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
        m_successors[from].push_back(to);
        m_predecessors[to].push_back(from);
    }

    template <typename N, typename G>
    template <class V>
    inline void DiGraphBase<N, G>::depth_first_search(V& visitor, node_id node) const
    {
        if (!m_node_list.empty())
        {
            color_list colors(m_node_list.size(), color::white);
            depth_first_search_impl(visitor, node, colors);
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
                                                           color_list& colors) const
    {
        colors[node] = color::gray;
        visitor.start_node(node, *crtp_cast());
        for (auto child : m_successors[node])
        {
            visitor.start_edge(node, child, *crtp_cast());
            if (colors[child] == color::white)
            {
                visitor.tree_edge(node, child, *crtp_cast());
                depth_first_search_impl(visitor, child, colors);
            }
            else if (colors[child] == color::gray)
            {
                visitor.back_edge(node, child, *crtp_cast());
            }
            else
            {
                visitor.forward_or_cross_edge(node, child, *crtp_cast());
            }
            visitor.finish_edge(node, child, *crtp_cast());
        }
        colors[node] = color::black;
        visitor.finish_node(node, *crtp_cast());
    }

    /*********************************
     *  DiGraph Edge Implementation  *
     *********************************/

    template <typename N, typename E>
    void DiGraph<N, E>::add_edge(node_id from, node_id to, edge_t const& data)
    {
        add_edge_impl(from, to, data);
    }

    template <typename N, typename E>
    void DiGraph<N, E>::add_edge(node_id from, node_id to, edge_t&& data)
    {
        add_edge_impl(from, to, std::move(data));
    }

    template <typename N, typename E>
    template <typename T>
    void DiGraph<N, E>::add_edge_impl(node_id from, node_id to, T&& data)
    {
        Base::add_edge(from, to);
        m_edges[{ from, to }] = std::forward<T>(data);
    }

    template <typename N, typename E>
    auto DiGraph<N, E>::edges() const -> const edge_map&
    {
        return m_edges;
    }

}  // namespace mamba

#endif  // INCLUDE_GRAPH_UTIL_HPP
