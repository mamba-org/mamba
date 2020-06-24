#include <gtest/gtest.h>

#include "graph_util.hpp"

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

        g.add_edge(0u, 1u);
        g.add_edge(0u, 2u);
        g.add_edge(1u, 3u);
        g.add_edge(1u, 4u);
        g.add_edge(2u, 3u);
        g.add_edge(2u, 5u);

        return g;
    }

    TEST(graph, build)
    {
        auto g = build_graph();
        EXPECT_EQ(g.get_node_list(), graph<int>::node_list({0, 1, 2, 3, 4, 5}));
    }
}

