#include <type_traits>

#include <gtest/gtest.h>

#include "mamba/core/util_graph.hpp"

namespace mamba
{

    TEST(vector_set, constructor)
    {
        const auto s1 = vector_set<int>();
        EXPECT_EQ(s1.size(), 0);
        auto s2 = vector_set<int>({ 1, 2 });
        EXPECT_EQ(s2.size(), 2);
        const auto s3 = vector_set<int>{ s2 };
        EXPECT_EQ(s3.size(), 2);
        const auto s4 = vector_set<int>{ std::move(s2) };
        EXPECT_EQ(s4.size(), 2);
        // CTAD
        auto s5 = vector_set({ 1, 2 });
        EXPECT_EQ(s5.size(), 2);
        static_assert(std::is_same_v<decltype(s5)::value_type, int>);
        auto s6 = vector_set(s5.begin(), s5.end(), std::greater{});
        EXPECT_EQ(s6.size(), s5.size());
        static_assert(std::is_same_v<decltype(s6)::value_type, decltype(s5)::value_type>);
    }

    TEST(vector_set, equality)
    {
        EXPECT_EQ(vector_set<int>(), vector_set<int>());
        EXPECT_EQ(vector_set<int>({ 1, 2 }), vector_set<int>({ 1, 2 }));
        EXPECT_EQ(vector_set<int>({ 1, 2 }), vector_set<int>({ 2, 1 }));
        EXPECT_EQ(vector_set<int>({ 1, 2, 1 }), vector_set<int>({ 2, 2, 1 }));
        EXPECT_NE(vector_set<int>({ 1, 2 }), vector_set<int>({ 1, 2, 3 }));
        EXPECT_NE(vector_set<int>({ 2 }), vector_set<int>({}));
    }

    TEST(vector_set, insert)
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
        auto v = std::vector<int>({ 33, 22, 17, 0 });
        s.insert(v.begin(), v.end());
        EXPECT_EQ(s, vector_set<int>({ 0, 17, 22, 33 }));
    }

    TEST(vector_set, erase)
    {
        auto s = vector_set<int>{ 4, 3, 2, 1 };
        EXPECT_EQ(s.erase(4), 1);
        EXPECT_EQ(s, vector_set<int>({ 1, 2, 3 }));
        EXPECT_EQ(s.erase(4), 0);
        EXPECT_EQ(s, vector_set<int>({ 1, 2, 3 }));

        const auto it = s.erase(s.begin());
        EXPECT_EQ(it, s.begin());
        EXPECT_EQ(s, vector_set<int>({ 2, 3 }));
    }

    TEST(vector_set, contains)
    {
        const auto s = vector_set<int>({ 1, 3, 4, 5 });
        EXPECT_FALSE(s.contains(0));
        EXPECT_TRUE(s.contains(1));
        EXPECT_FALSE(s.contains(2));
        EXPECT_TRUE(s.contains(3));
        EXPECT_TRUE(s.contains(4));
        EXPECT_TRUE(s.contains(5));
        EXPECT_FALSE(s.contains(6));
    }

    TEST(vector_set, key_compare)
    {
        auto s = vector_set({ 1, 3, 4, 5 }, std::greater{});
        EXPECT_EQ(s.front(), 5);
        EXPECT_EQ(s.back(), 1);
        s.insert(6);
        EXPECT_EQ(s.front(), 6);
    }

    DiGraph<double> build_graph()
    {
        DiGraph<double> g;
        const auto n0 = g.add_node(0.5);
        const auto n1 = g.add_node(1.5);
        const auto n2 = g.add_node(2.5);
        const auto n3 = g.add_node(3.5);
        const auto n4 = g.add_node(4.5);
        const auto n5 = g.add_node(5.5);
        const auto n6 = g.add_node(6.5);

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
        const auto n0 = g.add_node(0.5);
        const auto n1 = g.add_node(1.5);
        const auto n2 = g.add_node(2.5);
        const auto n3 = g.add_node(3.5);
        const auto n4 = g.add_node(4.5);

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
        const auto n0 = g.add_node(0.5);
        const auto n1 = g.add_node(1.5);
        const auto n2 = g.add_node(2.5);
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
        const auto g = build_graph();
        using node_map = decltype(g)::node_map;
        using node_id_list = decltype(g)::node_id_list;
        EXPECT_EQ(g.number_of_nodes(), 7ul);
        EXPECT_EQ(g.number_of_edges(), 7ul);
        EXPECT_EQ(
            g.nodes(),
            node_map(
                { { 0, 0.5 }, { 1, 1.5 }, { 2, 2.5 }, { 3, 3.5 }, { 4, 4.5 }, { 5, 5.5 }, { 6, 6.5 } }
            )
        );
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
        const auto g = build_edge_data_graph();
        using node_map = decltype(g)::node_map;
        using node_id_list = decltype(g)::node_id_list;
        EXPECT_EQ(g.number_of_nodes(), 3ul);
        EXPECT_EQ(g.number_of_edges(), 2ul);
        EXPECT_EQ(g.nodes(), node_map({ { 0, 0.5 }, { 1, 1.5 }, { 2, 2.5 } }));
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
        const auto g = build_graph();
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

        static constexpr auto new_node_val = -1.5;
        EXPECT_NE(g.node(0ul), new_node_val);
        g.node(0ul) = new_node_val;
        EXPECT_EQ(g.node(0ul), new_node_val);

        static constexpr auto new_edge_val = "data";
        EXPECT_NE(g.edge(0ul, 1ul), new_edge_val);
        g.edge(0ul, 1ul) = new_edge_val;
        EXPECT_EQ(g.edge(0ul, 1ul), new_edge_val);
    }

    TEST(graph, remove_edge)
    {
        auto g = build_edge_data_graph();
        const auto n_edges_init = g.number_of_edges();

        ASSERT_FALSE(g.has_edge(1, 0));
        ASSERT_TRUE(g.has_edge(0, 1));
        g.remove_edge(1, 0);
        EXPECT_EQ(g.number_of_edges(), n_edges_init);
        EXPECT_FALSE(g.has_edge(1, 0));
        EXPECT_TRUE(g.has_edge(0, 1));

        ASSERT_TRUE(g.has_edge(0, 1));
        g.remove_edge(0, 1);
        EXPECT_EQ(g.number_of_edges(), n_edges_init - 1u);
        EXPECT_FALSE(g.has_edge(0, 1));
        EXPECT_EQ(g.edges().count({ 0, 1 }), 0);
    }

    TEST(graph, degree)
    {
        const auto g = build_graph();
        EXPECT_EQ(g.out_degree(0), 2);
        EXPECT_EQ(g.out_degree(1), 2);
        EXPECT_EQ(g.out_degree(6), 0);
        EXPECT_EQ(g.in_degree(0), 0);
        EXPECT_EQ(g.in_degree(3), 2);
        EXPECT_EQ(g.in_degree(6), 1);
    }

    TEST(graph, for_each_node)
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        std::size_t n_nodes = 0;
        g.for_each_node_id(
            [&](node_id id)
            {
                EXPECT_TRUE(g.has_node(id));
                ++n_nodes;
            }
        );
        EXPECT_EQ(n_nodes, g.number_of_nodes());
    }

    TEST(graph, for_each_edge)
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        std::size_t n_edges = 0;
        g.for_each_edge_id(
            [&g, &n_edges](node_id from, node_id to)
            {
                EXPECT_TRUE(g.has_edge(from, to));
                ++n_edges;
            }
        );
        EXPECT_EQ(n_edges, g.number_of_edges());
    }

    TEST(graph, for_each_leaf)
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_leaf_id([&leaves](node_id leaf) { leaves.insert(leaf); });
        EXPECT_EQ(leaves, node_id_list({ 4ul, 5ul, 6ul }));
    }

    TEST(graph, for_each_leaf_from)
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_leaf_id_from(2ul, [&leaves](node_id leaf) { leaves.insert(leaf); });
        EXPECT_EQ(leaves, node_id_list({ 5ul, 6ul }));
    }

    TEST(graph, for_each_root)
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto roots = node_id_list();
        g.for_each_root_id([&roots](node_id root) { roots.insert(root); });
        EXPECT_EQ(roots, node_id_list({ 0ul }));
    }

    TEST(graph, for_each_root_from)
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_root_id_from(2ul, [&leaves](node_id leaf) { leaves.insert(leaf); });
        EXPECT_EQ(leaves, node_id_list({ 0ul }));
    }

    TEST(graph, depth_first_search)
    {
        const auto g = build_graph();
        test_visitor<DiGraph<double>> vis;
        g.depth_first_search(vis);
        EXPECT_TRUE(vis.get_back_edge_map().empty());
        EXPECT_EQ(vis.get_cross_edge_map().find(2u)->second, 3u);
    }

    TEST(graph, dfs_cyclic)
    {
        const auto g = build_cyclic_graph();
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
