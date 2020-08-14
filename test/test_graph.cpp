#include <gtest/gtest.h>

#include "mamba/graph_util.hpp"

namespace mamba
{
    graph<int> build_graph()
    {
        graph<int> g;
        g.add_node(0);
        g.add_node(1);
        g.add_node(2);
        g.add_node(3);
        g.add_node(4);
        g.add_node(5);
        g.add_node(6);

        g.add_edge(0u, 1u);
        g.add_edge(0u, 2u);
        g.add_edge(1u, 3u);
        g.add_edge(1u, 4u);
        g.add_edge(2u, 3u);
        g.add_edge(2u, 5u);
        g.add_edge(3u, 6u);

        return g;
    }

    graph<int> build_cyclic_graph()
    {
        graph<int> g;
        g.add_node(0);
        g.add_node(1);
        g.add_node(2);
        g.add_node(3);
        g.add_node(4);

        g.add_edge(0u, 1u);
        g.add_edge(0u, 3u);
        g.add_edge(1u, 2u);
        g.add_edge(2u, 0u);
        g.add_edge(3u, 4u);

        return g;
    }

    template <class G>
    class test_visitor : private default_visitor<G>
    {
    public:
        using base_type = default_visitor<G>;
        using node_id = typename base_type::node_id;
        using predecessor_map = std::map<node_id, node_id>;
        using edge_map = std::map<node_id, node_id>;

        using base_type::finish_edge;
        using base_type::finish_node;
        using base_type::start_edge;
        using base_type::start_node;
        using base_type::tree_edge;

        void back_edge(node_id from, node_id to, const G&)
        {
            m_back_edges[from] = to;
        }

        void forward_or_cross_edge(node_id from, node_id to, const G&)
        {
            m_cross_edges[from] = to;
        }

        const edge_map& get_back_edge_map() const
        {
            return m_back_edges;
        }

        const edge_map get_cross_edge_map() const
        {
            return m_cross_edges;
        }

    private:
        edge_map m_back_edges;
        edge_map m_cross_edges;
    };

    TEST(graph, build)
    {
        using node_list = graph<int>::node_list;
        using edge_list = graph<int>::edge_list;
        auto g = build_graph();
        EXPECT_EQ(g.get_node_list(), node_list({ 0u, 1u, 2u, 3u, 4u, 5u, 6u }));
        EXPECT_EQ(g.get_edge_list(0u), edge_list({ 1u, 2u }));
        EXPECT_EQ(g.get_edge_list(1u), edge_list({ 3u, 4u }));
        EXPECT_EQ(g.get_edge_list(2u), edge_list({ 3u, 5u }));
        EXPECT_EQ(g.get_edge_list(3u), edge_list({ 6u }));
    }

    TEST(graph, depth_first_search)
    {
        auto g = build_graph();
        test_visitor<graph<int>> vis;
        g.depth_first_search(vis);
        EXPECT_TRUE(vis.get_back_edge_map().empty());
        EXPECT_EQ(vis.get_cross_edge_map().find(2u)->second, 3u);
    }

    TEST(graph, dfs_cyclic)
    {
        auto g = build_cyclic_graph();
        test_visitor<graph<int>> vis;
        g.depth_first_search(vis);
        EXPECT_EQ(vis.get_back_edge_map().find(2u)->second, 0u);
        EXPECT_TRUE(vis.get_cross_edge_map().empty());
    }

    TEST(graph, dfs_empty)
    {
        graph<int> g;
        test_visitor<graph<int>> vis;
        g.depth_first_search(vis);
        EXPECT_TRUE(vis.get_back_edge_map().empty());
        EXPECT_TRUE(vis.get_cross_edge_map().empty());
    }
}  // namespace mamba
