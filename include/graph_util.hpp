#ifndef MAMBA_GRAPH_UTIL_HPP
#define MAMBA_GRAPH_UTIL_HPP

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
        void depth_first_search(V& visitor) const;

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

    /************************
     * graph implementation *
     ************************/

    template <class T>
    inline auto graph<T>::get_node_list() const -> const node_list&
    {
        return m_node_list;
    }

    template <class T>
    inline auto graph<T>::get_edge_list(node_id id) const  -> const edge_list&
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
    inline void graph<T>::depth_first_search(V& visitor) const
    {
        color_list colors(m_node_list.size(), color::white);
        depth_first_search_impl(visitor, node_id(0), colors);
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
    inline void graph<T>::depth_first_search_impl(V& visitor, node_id node, color_list& colors) const
    {
        colors[node] = color::gray;
        visitor.start_node(node);
        for (auto child: m_adjacency_list[node])
        {
            visitor.start_edge(node, child);
            if (colors[child] == color::white)
            {
                visitor.tree_edge(node, child);
                depth_first_search_impl(visitor, child, colors);
            }
            else if (colors[child] == color::gray)
            {
                visitor.back_edge(node, child);
            }
            else
            {
                visitor.forward_or_cross_edge(node, child);
            }
            visitor.finish_edge(node, child);
        }
        colors[node] = color::black;
        visitor.finish_node(node);
    }
}

#endif

