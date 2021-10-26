// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_GRAPH_UTIL_HPP
#define MAMBA_CORE_GRAPH_UTIL_HPP

#include <map>
#include <utility>
#include <vector>

namespace mamba
{
    // Simplified implementation of a directed graph
    // where a path exists between each node and the
    // first one (you can think of it as a tree with
    // potential cycles)

    template <class T>
    class graph
    {
    public:
        using node = T;
        using node_id = size_t;
        using node_list = std::vector<node>;
        using edge_list = std::vector<node_id>;
        using adjacency_list = std::vector<edge_list>;
        using cycle_list = std::vector<node_list>;

        const node_list& get_node_list() const;
        const edge_list& get_edge_list(node_id id) const;

        node_id add_node(const node& value);
        node_id add_node(node&& value);
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
        adjacency_list m_adjacency_list;
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

    template <class G>
    class predecessor_recorder : private default_visitor<G>
    {
    public:
        using base_type = default_visitor<G>;
        using graph_type = typename base_type::graph_type;
        using node_id = typename base_type::node_it;
        using predecessor_map = std::map<node_id, node_id>;

        using base_type::back_edge;
        using base_type::finish_node;
        using base_type::forward_or_cross_edge;
        using base_type::start_edge;
        using base_type::start_node;

        void tree_edge(node_id from, node_id to, const G&);
        void finish_edge(node_id id, const G&);

        const predecessor_map& get_predecessors() const;

    private:
        predecessor_map m_pred;
    };

    template <class G, template <class> class V1, template <class> class V2>
    class composite_visitor
    {
    public:
        using graph_type = G;
        using node_id = typename G::node_id;

        composite_visitor(V1<G> v1, V2<G> v2);

        void start_node(node_id, const G&);
        void finish_node(node_id, const G&);

        void start_edge(node_id, node_id, const G&);
        void tree_edge(node_id, node_id, const G&);
        void back_edge(node_id, node_id, const G&);
        void forward_or_cross_edge(node_id, node_id, const G&);
        void finish_edge(node_id, node_id, const G&);

    private:
        V1<G> m_v1;
        V2<G> m_v2;
    };

    /************************
     * graph implementation *
     ************************/

    template <class T>
    inline auto graph<T>::get_node_list() const -> const node_list&
    {
        return m_node_list;
    }

    template <class T>
    inline auto graph<T>::get_edge_list(node_id id) const -> const edge_list&
    {
        return m_adjacency_list[id];
    }

    template <class T>
    inline auto graph<T>::add_node(const node& value) -> node_id
    {
        return add_node_impl(value);
    }

    template <class T>
    inline auto graph<T>::add_node(node&& value) -> node_id
    {
        return add_node_impl(std::move(value));
    }

    template <class T>
    inline void graph<T>::add_edge(node_id from, node_id to)
    {
        m_adjacency_list[from].push_back(to);
    }

    template <class T>
    template <class V>
    inline void graph<T>::depth_first_search(V& visitor, node_id node) const
    {
        if (!m_node_list.empty())
        {
            color_list colors(m_node_list.size(), color::white);
            depth_first_search_impl(visitor, node, colors);
        }
    }

    template <class T>
    template <class V>
    inline auto graph<T>::add_node_impl(V&& value) -> node_id
    {
        m_node_list.push_back(std::forward<V>(value));
        m_adjacency_list.push_back(edge_list());
        return m_node_list.size() - 1u;
    }

    template <class T>
    template <class V>
    inline void graph<T>::depth_first_search_impl(V& visitor,
                                                  node_id node,
                                                  color_list& colors) const
    {
        colors[node] = color::gray;
        visitor.start_node(node, *this);
        for (auto child : m_adjacency_list[node])
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

    /***************************************
     * predecessor_recorder implementation *
     ***************************************/

    template <class G>
    inline void predecessor_recorder<G>::tree_edge(node_id from, node_id to, const G&)
    {
        m_pred[to] = from;
    }

    template <class G>
    inline void predecessor_recorder<G>::finish_edge(node_id id, const G&)
    {
        m_pred.erase(id);
    }

    template <class G>
    inline auto predecessor_recorder<G>::get_predecessors() const -> const predecessor_map&
    {
        return m_pred;
    }

    /************************************
     * composite_visitor implementation *
     ************************************/

    template <class G, template <class> class V1, template <class> class V2>
    inline composite_visitor<G, V1, V2>::composite_visitor(V1<G> v1, V2<G> v2)
        : m_v1(v1)
        , m_v2(v2)
    {
    }

    template <class G, template <class> class V1, template <class> class V2>
    inline void composite_visitor<G, V1, V2>::start_node(node_id id, const G& g)
    {
        m_v1.start_node(id, g);
        m_v2.start_node(id, g);
    }

    template <class G, template <class> class V1, template <class> class V2>
    inline void composite_visitor<G, V1, V2>::finish_node(node_id id, const G& g)
    {
        m_v1.finish_node(id, g);
        m_v2.finish_node(id, g);
    }

    template <class G, template <class> class V1, template <class> class V2>
    inline void composite_visitor<G, V1, V2>::start_edge(node_id from, node_id to, const G& g)
    {
        m_v1.start_edge(from, to, g);
        m_v2.start_edge(from, to, g);
    }

    template <class G, template <class> class V1, template <class> class V2>
    inline void composite_visitor<G, V1, V2>::tree_edge(node_id from, node_id to, const G& g)
    {
        m_v1.tree_edge(from, to, g);
        m_v2.tree_edge(from, to, g);
    }

    template <class G, template <class> class V1, template <class> class V2>
    inline void composite_visitor<G, V1, V2>::back_edge(node_id from, node_id to, const G& g)
    {
        m_v1.back_edge(from, to, g);
        m_v2.back_edge(from, to, g);
    }

    template <class G, template <class> class V1, template <class> class V2>
    inline void composite_visitor<G, V1, V2>::forward_or_cross_edge(node_id from,
                                                                    node_id to,
                                                                    const G& g)
    {
        m_v1.forward_or_cross_edge(from, to, g);
        m_v2.forward_or_cross_edge(from, to, g);
    }

    template <class G, template <class> class V1, template <class> class V2>
    inline void composite_visitor<G, V1, V2>::finish_edge(node_id from, node_id to, const G& g)
    {
        m_v1.finish_edge(from, to, g);
        m_v2.finish_edge(from, to, g);
    }
}  // namespace mamba

#endif  // INCLUDE_GRAPH_UTIL_HPP
