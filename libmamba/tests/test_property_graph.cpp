#include <gtest/gtest.h>

#include "mamba/core/property_graph.hpp"

namespace mamba
{
    template <class T>
    struct Info
    {
    public:
        std::vector<T> m_values;
        void add(T a);
        Info(T a);
        Info();
    };

    template <class T>
    inline void Info<T>::add(T a)
    {
        m_values.push_back(a);
    }

    template <class T>
    inline Info<T>::Info(T a)
    {
        m_values.push_back(a);
    }

    template <class T>
    inline Info<T>::Info()
    {
    }

    TEST(problems_graph, test_leaves_to_roots)
    {
        MPropertyGraph<int, std::string> g;
        size_t node_zero = g.add_node(0);
        size_t node_one = g.add_node(1);
        size_t node_two = g.add_node(2);
        size_t node_three = g.add_node(3);
        size_t node_four = g.add_node(4);
        size_t node_five = g.add_node(5);
        size_t node_six = g.add_node(6);
        size_t node_seven = g.add_node(7);

        g.add_edge(node_zero, node_one, "one");
        g.add_edge(node_zero, node_five, "five");
        g.add_edge(node_one, node_three, "a");
        g.add_edge(node_one, node_two, "b");
        g.add_edge(node_two, node_four, "c");
        g.add_edge(node_three, node_four, "d");

        g.add_edge(node_five, node_six, "e");
        g.add_edge(node_six, node_seven, "f");

        std::unordered_map<size_t, std::vector<std::pair<size_t, std::string>>> expected_value{
            { node_one,
              std::vector<std::pair<size_t, std::string>>{ std::make_pair(node_one, "one"),
                                                           std::make_pair(node_four, "d"),
                                                           std::make_pair(node_four, "c") } },
            { node_five,
              std::vector<std::pair<size_t, std::string>>{ std::make_pair(node_five, "five"),
                                                           std::make_pair(node_seven, "f") } }
        };
        // TODO cleanup comparison
        for (const auto& root_to_leaves : g.get_parents_to_leaves())
        {
            EXPECT_TRUE(expected_value.find(root_to_leaves.first) != expected_value.end());
            EXPECT_TRUE(root_to_leaves.second.size()
                        == expected_value[root_to_leaves.first].size());
            for (size_t i = 0; i < root_to_leaves.second.size(); ++i)
            {
                EXPECT_TRUE(root_to_leaves.second[i].first
                            == expected_value[root_to_leaves.first][i].first);
                EXPECT_TRUE(root_to_leaves.second[i].second
                            == expected_value[root_to_leaves.first][i].second);
            }
        }
    }

    TEST(problems_graph, test_update_nodes_edges_info)
    {
        MPropertyGraph<Info<std::string>, Info<std::string>> g;
        size_t root = g.add_node(Info<std::string>("root"));
        size_t a = g.add_node(Info<std::string>("a"));
        size_t b = g.add_node(Info<std::string>("b"));
        size_t c = g.add_node(Info<std::string>("c"));
        size_t d = g.add_node(Info<std::string>("d"));

        g.add_edge(root, a, Info<std::string>("a*"));
        g.add_edge(a, c, Info<std::string>("c*"));
        g.add_edge(c, d, Info<std::string>("d*"));
        g.add_edge(root, b, Info<std::string>("b*"));

        g.update_node<std::string>(a, "aa");
        g.update_node<std::string>(a, "aaa");
        g.update_node<std::string>(d, "dd");

        std::vector<std::string> a_values{ "a", "aa", "aaa" };

        EXPECT_TRUE(g.get_node(a).m_values == a_values);

        std::vector<std::string> d_values{ "d", "dd" };
        EXPECT_TRUE(g.get_node(d).m_values == d_values);

        EXPECT_TRUE(g.update_edge_if_present<std::string>(root, a, "a 1.0.0"));
        EXPECT_FALSE(g.update_edge_if_present<std::string>(root, d, "invalid*"));
        EXPECT_TRUE(g.update_edge_if_present<std::string>(root, a, "aaa*"));

        std::vector<std::pair<size_t, Info<std::string>>> root_edges = g.get_edge_list(root);
        std::vector<std::string> a_edges{ "a*", "a 1.0.0", "aaa*" };
        std::vector<std::string> b_edges{ "b*" };
        EXPECT_TRUE(root_edges[0].second.m_values == a_edges);
        EXPECT_TRUE(root_edges[1].second.m_values == b_edges);
    }
}
