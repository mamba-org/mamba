#include <gtest/gtest.h>

#include "mamba/core/graph_util.hpp"

namespace mamba
{
    DiGraph<double> build_graph()
    {
        DiGraph<double> g;
        auto const n0 = g.add_node(0.5);
        auto const n1 = g.add_node(1.5);
        auto const n2 = g.add_node(2.5);
        auto const n3 = g.add_node(3.5);
        auto const n4 = g.add_node(4.5);
        auto const n5 = g.add_node(5.5);
        auto const n6 = g.add_node(6.5);

        g.add_edge(n0, n1);
        g.add_edge(n0, n2);
        g.add_edge(n1, n3);
        g.add_edge(n1, n4);
        g.add_edge(n2, n3);
        g.add_edge(n2, n5);
        g.add_edge(n3, n6);

        return g;
    }

    DiGraph<double> build_cyclic_graph()
    {
        DiGraph<double> g;
        auto const n0 = g.add_node(0.5);
        auto const n1 = g.add_node(1.5);
        auto const n2 = g.add_node(2.5);
        auto const n3 = g.add_node(3.5);
        auto const n4 = g.add_node(4.5);

        g.add_edge(n0, n1);
        g.add_edge(n0, n3);
        g.add_edge(n1, n2);
        g.add_edge(n2, n0);
        g.add_edge(n3, n4);

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
        auto const g = build_graph();
        using node_list = decltype(g)::node_list;
        using edge_list = decltype(g)::edge_list;
        EXPECT_EQ(g.number_of_nodes(), 7ul);
        EXPECT_EQ(g.get_node_list(), node_list({ 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5 }));
        EXPECT_EQ(g.successors(0u), edge_list({ 1u, 2u }));
        EXPECT_EQ(g.successors(1u), edge_list({ 3u, 4u }));
        EXPECT_EQ(g.successors(2u), edge_list({ 3u, 5u }));
        EXPECT_EQ(g.successors(3u), edge_list({ 6u }));
        EXPECT_EQ(g.predecessors(0u), edge_list());
        EXPECT_EQ(g.predecessors(1u), edge_list({ 0u }));
        EXPECT_EQ(g.predecessors(2u), edge_list({ 0u }));
        EXPECT_EQ(g.predecessors(3u), edge_list({ 1u, 2u }));
    }

    TEST(graph, depth_first_search)
    {
        auto const g = build_graph();
        test_visitor<DiGraph<double>> vis;
        g.depth_first_search(vis);
        EXPECT_TRUE(vis.get_back_edge_map().empty());
        EXPECT_EQ(vis.get_cross_edge_map().find(2u)->second, 3u);
    }

    TEST(graph, dfs_cyclic)
    {
        auto const g = build_cyclic_graph();
        test_visitor<DiGraph<double>> vis;
        g.depth_first_search(vis);
        EXPECT_EQ(vis.get_back_edge_map().find(2u)->second, 0u);
        EXPECT_TRUE(vis.get_cross_edge_map().empty());
    }

    TEST(graph, dfs_empty)
    {
        DiGraph<int> g;
        test_visitor<DiGraph<int>> vis;
        g.depth_first_search(vis);
        EXPECT_TRUE(vis.get_back_edge_map().empty());
        EXPECT_TRUE(vis.get_cross_edge_map().empty());
    }
}  // namespace mamba
