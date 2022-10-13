#include <gtest/gtest.h>

#include "mamba/core/graph_util.hpp"

namespace mamba
{

    TEST(vector_set, constructor)
    {
        auto const s1 = vector_set<int>();
        EXPECT_EQ(s1.size(), 0);
        auto s2 = vector_set<int>({ 1, 2 });
        EXPECT_EQ(s2.size(), 2);
        auto const s3 = vector_set<int>{ s2 };
        EXPECT_EQ(s3.size(), 2);
        auto const s4 = vector_set<int>{ std::move(s2) };
        EXPECT_EQ(s4.size(), 2);
    }

    TEST(vector_set, equality)
    {
        EXPECT_EQ(vector_set<int>(), vector_set<int>());
        EXPECT_EQ(vector_set<int>({ 1, 2 }), vector_set<int>({ 1, 2 }));
        EXPECT_EQ(vector_set<int>({ 1, 2 }), vector_set<int>({ 2, 1 }));
        EXPECT_EQ(vector_set<int>({ 1, 2, 1 }), vector_set<int>({ 2, 2, 1 }));
    }

    TEST(vector_set, insertion)
    {
        auto s = vector_set<int>();
        s.insert(33);
        EXPECT_EQ(s, vector_set<int>({ 33 }));
        s.insert(33);
        s.insert(17);
        EXPECT_EQ(s, vector_set<int>({ 17, 33 }));
        s.insert(22);
        EXPECT_EQ(s, vector_set<int>({ 17, 22, 33 }));
        s.insert(33);
        EXPECT_EQ(s, vector_set<int>({ 17, 22, 33 }));
    }

    TEST(vector_set, contains)
    {
        auto const s = vector_set<int>({ 1, 3, 4, 5 });
        EXPECT_FALSE(s.contains(0));
        EXPECT_TRUE(s.contains(1));
        EXPECT_FALSE(s.contains(2));
        EXPECT_TRUE(s.contains(3));
        EXPECT_TRUE(s.contains(4));
        EXPECT_TRUE(s.contains(5));
        EXPECT_FALSE(s.contains(6));
    }

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

    DiGraph<double, const char*> build_edge_data_graph()
    {
        auto g = DiGraph<double, const char*>{};
        auto const n0 = g.add_node(0.5);
        auto const n1 = g.add_node(1.5);
        auto const n2 = g.add_node(2.5);
        g.add_edge(n0, n1, "n0->n1");
        g.add_edge(n1, n2, "n1->n2");
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

    TEST(graph, build_simple)
    {
        auto const g = build_graph();
        using node_list = decltype(g)::node_list;
        using node_id_list = decltype(g)::node_id_list;
        EXPECT_EQ(g.number_of_nodes(), 7ul);
        EXPECT_EQ(g.nodes(), node_list({ 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5 }));
        EXPECT_EQ(g.successors(0u), node_id_list({ 1u, 2u }));
        EXPECT_EQ(g.successors(1u), node_id_list({ 3u, 4u }));
        EXPECT_EQ(g.successors(2u), node_id_list({ 3u, 5u }));
        EXPECT_EQ(g.successors(3u), node_id_list({ 6u }));
        EXPECT_EQ(g.predecessors(0u), node_id_list());
        EXPECT_EQ(g.predecessors(1u), node_id_list({ 0u }));
        EXPECT_EQ(g.predecessors(2u), node_id_list({ 0u }));
        EXPECT_EQ(g.predecessors(3u), node_id_list({ 1u, 2u }));
    }

    TEST(graph, build_edge_data)
    {
        auto const g = build_edge_data_graph();
        using node_list = decltype(g)::node_list;
        using node_id_list = decltype(g)::node_id_list;
        EXPECT_EQ(g.number_of_nodes(), 3ul);
        EXPECT_EQ(g.nodes(), node_list({ 0.5, 1.5, 2.5 }));
        EXPECT_EQ(g.successors(0ul), node_id_list({ 1ul }));
        EXPECT_EQ(g.successors(1ul), node_id_list({ 2ul }));
        EXPECT_EQ(g.successors(2ul), node_id_list());
        EXPECT_EQ(g.predecessors(0ul), node_id_list());
        EXPECT_EQ(g.predecessors(1ul), node_id_list({ 0ul }));
        EXPECT_EQ(g.predecessors(2ul), node_id_list({ 1ul }));

        using edge_map = decltype(g)::edge_map;
        EXPECT_EQ(g.edges(), edge_map({ { { 0ul, 1ul }, "n0->n1" }, { { 1ul, 2ul }, "n1->n2" } }));
    }

    TEST(graph, has_node_edge)
    {
        auto const g = build_graph();
        EXPECT_TRUE(g.has_node(1ul));
        EXPECT_TRUE(g.has_node(4ul));
        EXPECT_FALSE(g.has_node(g.number_of_nodes()));
        EXPECT_TRUE(g.has_edge(1ul, 4ul));
        EXPECT_FALSE(g.has_edge(4ul, 1ul));
        EXPECT_TRUE(g.has_edge(0ul, 2ul));
        EXPECT_FALSE(g.has_edge(0ul, 5ul));
        EXPECT_FALSE(g.has_edge(0ul, g.number_of_nodes()));
        EXPECT_FALSE(g.has_edge(g.number_of_nodes(), 1ul));
    }

    TEST(graph, data_modifier)
    {
        auto g = build_edge_data_graph();

        auto static constexpr new_node_val = -1.5;
        EXPECT_NE(g.node(0ul), new_node_val);
        g.node(0ul) = new_node_val;
        EXPECT_EQ(g.node(0ul), new_node_val);

        auto static constexpr new_edge_val = "data";
        EXPECT_NE(g.edge(0ul, 1ul), new_edge_val);
        g.edge(0ul, 1ul) = new_edge_val;
        EXPECT_EQ(g.edge(0ul, 1ul), new_edge_val);
    }

    TEST(graph, degree)
    {
        auto const g = build_graph();
        EXPECT_EQ(g.out_degree(0), 2);
        EXPECT_EQ(g.out_degree(1), 2);
        EXPECT_EQ(g.out_degree(6), 0);
        EXPECT_EQ(g.in_degree(0), 0);
        EXPECT_EQ(g.in_degree(3), 2);
        EXPECT_EQ(g.in_degree(6), 1);
    }

    TEST(graph, for_each_leaf)
    {
        auto const g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_leaf([&leaves](node_id leaf) { leaves.insert(leaf); });
        EXPECT_EQ(leaves, node_id_list({ 4ul, 5ul, 6ul }));
    }

    TEST(graph, for_each_leaf_from)
    {
        auto const g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_leaf_from(2ul, [&leaves](node_id leaf) { leaves.insert(leaf); });
        EXPECT_EQ(leaves, node_id_list({ 5ul, 6ul }));
    }

    TEST(graph, for_each_root)
    {
        auto const g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto roots = node_id_list();
        g.for_each_root([&roots](node_id root) { roots.insert(root); });
        EXPECT_EQ(roots, node_id_list({ 0ul }));
    }

    TEST(graph, for_each_root_from)
    {
        auto const g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_root_from(2ul, [&leaves](node_id leaf) { leaves.insert(leaf); });
        EXPECT_EQ(leaves, node_id_list({ 0ul }));
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

    TEST(graph_algorithm, is_reachable)
    {
        auto graph = build_graph();
        EXPECT_TRUE(is_reachable(graph, 0, 6));
        EXPECT_FALSE(is_reachable(graph, 6, 0));
    }
}  // namespace mamba
