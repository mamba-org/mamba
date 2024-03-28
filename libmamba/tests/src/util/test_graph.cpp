// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include "mamba/util/graph.hpp"

using namespace mamba::util;

auto
build_graph() -> DiGraph<double>
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

auto
build_cyclic_graph() -> DiGraph<double>
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

auto
build_edge_data_graph() -> DiGraph<double, const char*>
{
    auto g = DiGraph<double, const char*>{};
    const auto n0 = g.add_node(0.5);
    const auto n1 = g.add_node(1.5);
    const auto n2 = g.add_node(2.5);
    g.add_edge(n0, n1, "n0->n1");
    g.add_edge(n1, n2, "n1->n2");
    return g;
}

template <typename Graph>
class test_visitor : private EmptyVisitor<Graph>
{
public:

    using base_type = EmptyVisitor<Graph>;
    using node_id = typename base_type::node_id;
    using node_id_list = std::vector<node_id>;
    using predecessor_map = std::map<node_id, node_id>;
    using edge_map = std::map<node_id, node_id>;

    using base_type::finish_edge;
    using base_type::start_edge;
    using base_type::tree_edge;

    void start_node(node_id n, const Graph&)
    {
        m_start_nodes.push_back(n);
    }

    void finish_node(node_id n, const Graph&)
    {
        m_finish_nodes.push_back(n);
    }

    void back_edge(node_id from, node_id to, const Graph&)
    {
        m_back_edges[from] = to;
    }

    void forward_or_cross_edge(node_id from, node_id to, const Graph&)
    {
        m_cross_edges[from] = to;
    }

    auto get_start_node_list() const -> const node_id_list&
    {
        return m_start_nodes;
    }

    auto get_finish_node_list() const -> const node_id_list&
    {
        return m_finish_nodes;
    }

    auto get_back_edge_map() const -> const edge_map&
    {
        return m_back_edges;
    }

    auto get_cross_edge_map() const -> const edge_map
    {
        return m_cross_edges;
    }

private:

    edge_map m_back_edges;
    edge_map m_cross_edges;
    node_id_list m_start_nodes;
    node_id_list m_finish_nodes;
};

TEST_SUITE("util::graph")
{
    TEST_CASE("build_simple")
    {
        const auto g = build_graph();
        using node_map = decltype(g)::node_map;
        using node_id_list = decltype(g)::node_id_list;
        CHECK_EQ(g.number_of_nodes(), 7ul);
        CHECK_EQ(g.number_of_edges(), 7ul);
        CHECK_EQ(
            g.nodes(),
            node_map(
                { { 0, 0.5 }, { 1, 1.5 }, { 2, 2.5 }, { 3, 3.5 }, { 4, 4.5 }, { 5, 5.5 }, { 6, 6.5 } }
            )
        );
        CHECK_EQ(g.successors(0u), node_id_list({ 1u, 2u }));
        CHECK_EQ(g.successors(1u), node_id_list({ 3u, 4u }));
        CHECK_EQ(g.successors(2u), node_id_list({ 3u, 5u }));
        CHECK_EQ(g.successors(3u), node_id_list({ 6u }));
        CHECK_EQ(g.predecessors(0u), node_id_list());
        CHECK_EQ(g.predecessors(1u), node_id_list({ 0u }));
        CHECK_EQ(g.predecessors(2u), node_id_list({ 0u }));
        CHECK_EQ(g.predecessors(3u), node_id_list({ 1u, 2u }));
    }

    TEST_CASE("build_edge_data")
    {
        const auto g = build_edge_data_graph();
        using node_map = decltype(g)::node_map;
        using node_id_list = decltype(g)::node_id_list;
        CHECK_EQ(g.number_of_nodes(), 3ul);
        CHECK_EQ(g.number_of_edges(), 2ul);
        CHECK_EQ(g.nodes(), node_map({ { 0, 0.5 }, { 1, 1.5 }, { 2, 2.5 } }));
        CHECK_EQ(g.successors(0ul), node_id_list({ 1ul }));
        CHECK_EQ(g.successors(1ul), node_id_list({ 2ul }));
        CHECK_EQ(g.successors(2ul), node_id_list());
        CHECK_EQ(g.predecessors(0ul), node_id_list());
        CHECK_EQ(g.predecessors(1ul), node_id_list({ 0ul }));
        CHECK_EQ(g.predecessors(2ul), node_id_list({ 1ul }));

        using edge_map = decltype(g)::edge_map;
        CHECK_EQ(g.edges(), edge_map({ { { 0ul, 1ul }, "n0->n1" }, { { 1ul, 2ul }, "n1->n2" } }));
    }

    TEST_CASE("has_node_edge")
    {
        const auto g = build_graph();
        CHECK(g.has_node(1ul));
        CHECK(g.has_node(4ul));
        CHECK_FALSE(g.has_node(g.number_of_nodes()));
        CHECK(g.has_edge(1ul, 4ul));
        CHECK_FALSE(g.has_edge(4ul, 1ul));
        CHECK(g.has_edge(0ul, 2ul));
        CHECK_FALSE(g.has_edge(0ul, 5ul));
        CHECK_FALSE(g.has_edge(0ul, g.number_of_nodes()));
        CHECK_FALSE(g.has_edge(g.number_of_nodes(), 1ul));
    }

    TEST_CASE("data_modifier")
    {
        auto g = build_edge_data_graph();

        static constexpr auto new_node_val = -1.5;
        CHECK_NE(g.node(0ul), new_node_val);
        g.node(0ul) = new_node_val;
        CHECK_EQ(g.node(0ul), new_node_val);

        static constexpr auto new_edge_val = "data";
        CHECK_NE(g.edge(0ul, 1ul), new_edge_val);
        g.edge(0ul, 1ul) = new_edge_val;
        CHECK_EQ(g.edge(0ul, 1ul), new_edge_val);
    }

    TEST_CASE("remove_edge")
    {
        auto g = build_edge_data_graph();
        const auto n_edges_init = g.number_of_edges();

        REQUIRE_FALSE(g.has_edge(1, 0));
        REQUIRE(g.has_edge(0, 1));
        CHECK_FALSE(g.remove_edge(1, 0));
        CHECK_EQ(g.number_of_edges(), n_edges_init);
        CHECK_FALSE(g.has_edge(1, 0));
        CHECK(g.has_edge(0, 1));

        REQUIRE(g.has_edge(0, 1));
        CHECK(g.remove_edge(0, 1));
        CHECK_EQ(g.number_of_edges(), n_edges_init - 1u);
        CHECK_FALSE(g.has_edge(0, 1));
        CHECK_EQ(g.edges().count({ 0, 1 }), 0);
    }

    TEST_CASE("remove_node")
    {
        auto g = build_edge_data_graph();

        REQUIRE(g.has_node(0));
        REQUIRE(g.has_node(1));
        REQUIRE(g.has_node(2));
        REQUIRE(g.has_edge(0, 1));
        REQUIRE(g.has_edge(1, 2));

        const auto n_edges_init = g.number_of_edges();
        const auto n_nodes_init = g.number_of_nodes();
        const auto node_1_degree = g.in_degree(1) + g.out_degree(1);

        CHECK(g.remove_node(1));
        CHECK_EQ(g.number_of_nodes(), n_nodes_init - 1u);
        CHECK_EQ(g.number_of_edges(), n_edges_init - node_1_degree);
        CHECK_EQ(g.number_of_edges(), g.edges().size());
        CHECK(g.has_node(0));
        CHECK_FALSE(g.has_node(1));
        CHECK(g.has_node(2));
        CHECK_EQ(g.in_degree(1), 0);
        CHECK_EQ(g.out_degree(1), 0);
        CHECK_FALSE(g.has_edge(0, 1));
        CHECK_FALSE(g.has_edge(1, 2));
        g.for_each_node_id([&](auto id) { CHECK(g.has_node(id)); });

        CHECK_FALSE(g.remove_node(1));
        CHECK_EQ(g.number_of_nodes(), n_nodes_init - 1u);
        CHECK_EQ(g.number_of_edges(), n_edges_init - node_1_degree);
        CHECK_EQ(g.number_of_edges(), g.edges().size());

        const auto new_id = g.add_node(.7);
        CHECK_EQ(new_id, n_nodes_init);  // Ids are not invalidated so new id is used
        CHECK_FALSE(g.has_node(1));      // Old id is not being confused
        CHECK_EQ(g.number_of_nodes(), n_nodes_init);
    }

    TEST_CASE("degree")
    {
        const auto g = build_graph();
        CHECK_EQ(g.out_degree(0), 2);
        CHECK_EQ(g.out_degree(1), 2);
        CHECK_EQ(g.out_degree(6), 0);
        CHECK_EQ(g.in_degree(0), 0);
        CHECK_EQ(g.in_degree(3), 2);
        CHECK_EQ(g.in_degree(6), 1);
    }

    TEST_CASE("for_each_node")
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        std::size_t n_nodes = 0;
        g.for_each_node_id(
            [&](node_id id)
            {
                CHECK(g.has_node(id));
                ++n_nodes;
            }
        );
        CHECK_EQ(n_nodes, g.number_of_nodes());
    }

    TEST_CASE("for_each_edge")
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        std::size_t n_edges = 0;
        g.for_each_edge_id(
            [&g, &n_edges](node_id from, node_id to)
            {
                CHECK(g.has_edge(from, to));
                ++n_edges;
            }
        );
        CHECK_EQ(n_edges, g.number_of_edges());
    }

    TEST_CASE("for_each_leaf")
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_leaf_id([&leaves](node_id leaf) { leaves.insert(leaf); });
        CHECK_EQ(leaves, node_id_list({ 4ul, 5ul, 6ul }));
    }

    TEST_CASE("for_each_leaf_from")
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_leaf_id_from(2ul, [&leaves](node_id leaf) { leaves.insert(leaf); });
        CHECK_EQ(leaves, node_id_list({ 5ul, 6ul }));
    }

    TEST_CASE("for_each_root")
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto roots = node_id_list();
        g.for_each_root_id([&roots](node_id root) { roots.insert(root); });
        CHECK_EQ(roots, node_id_list({ 0ul }));
    }

    TEST_CASE("for_each_root_from")
    {
        const auto g = build_graph();
        using node_id = decltype(g)::node_id;
        using node_id_list = decltype(g)::node_id_list;
        auto leaves = node_id_list();
        g.for_each_root_id_from(2ul, [&leaves](node_id leaf) { leaves.insert(leaf); });
        CHECK_EQ(leaves, node_id_list({ 0ul }));
    }

    TEST_CASE("depth_first_search")
    {
        const auto g = build_graph();
        test_visitor<DiGraph<double>> vis;
        using node_id = typename decltype(g)::node_id;
        dfs_raw(g, vis, /* start= */ node_id(0));
        CHECK(vis.get_back_edge_map().empty());
        CHECK_EQ(vis.get_cross_edge_map().find(2u)->second, 3u);

        const auto& start_node_list = vis.get_start_node_list();
        const auto& finish_node_list = vis.get_finish_node_list();
        CHECK_FALSE(start_node_list.empty());
        CHECK_FALSE(finish_node_list.empty());
        const auto start_node_set = std::set(start_node_list.begin(), start_node_list.end());
        CHECK_EQ(start_node_list.size(), start_node_set.size());  // uniqueness
        const auto finish_node_set = std::set(finish_node_list.begin(), finish_node_list.end());
        CHECK_EQ(finish_node_list.size(), finish_node_set.size());  // uniqueness
        CHECK_EQ(start_node_set, finish_node_set);
    }

    TEST_CASE("dfs_cyclic")
    {
        const auto g = build_cyclic_graph();
        test_visitor<DiGraph<double>> vis;
        using node_id = typename decltype(g)::node_id;
        dfs_raw(g, vis, /* start= */ node_id(0));
        CHECK_EQ(vis.get_back_edge_map().find(2u)->second, 0u);
        CHECK(vis.get_cross_edge_map().empty());
    }

    TEST_CASE("dfs_empty")
    {
        DiGraph<int> g;
        test_visitor<DiGraph<int>> vis;
        using node_id = typename decltype(g)::node_id;
        dfs_raw(g, vis, /* start= */ node_id(0));
        CHECK(vis.get_back_edge_map().empty());
        CHECK(vis.get_cross_edge_map().empty());
    }

    template <typename Graph, typename Iter>
    auto is_node_id_permutation(const Graph& g, Iter first, Iter last) -> bool
    {
        using node_id = typename Graph::node_id;
        auto node_ids = std::vector<node_id>();
        g.for_each_node_id([&node_ids](node_id n) { node_ids.push_back(n); });
        return std::is_permutation(first, last, node_ids.cbegin(), node_ids.cend());
    }

    TEST_CASE("dfs_all")
    {
        DiGraph<double> g;
        const auto n0 = g.add_node(0);
        const auto n1 = g.add_node(1);
        const auto n2 = g.add_node(2);
        g.add_edge(n0, n1);
        g.add_edge(n2, n1);

        test_visitor<decltype(g)> vis = {};
        dfs_raw(g, vis);


        const auto& start_node_list = vis.get_start_node_list();
        CHECK(is_node_id_permutation(g, start_node_list.cbegin(), start_node_list.cend()));
        const auto& finish_node_list = vis.get_finish_node_list();
        CHECK(is_node_id_permutation(g, finish_node_list.cbegin(), finish_node_list.cend()));
        const auto start_node_set = std::set(start_node_list.begin(), start_node_list.end());
        const auto finish_node_set = std::set(finish_node_list.begin(), finish_node_list.end());
        CHECK_EQ(start_node_set, finish_node_set);
        CHECK_EQ(start_node_set.size(), 3);
    }

    TEST_CASE("dfs_preorder & dfs_postorder")
    {
        DiGraph<double> g;
        const auto n0 = g.add_node(0);
        const auto n1 = g.add_node(1);
        const auto n2 = g.add_node(2);
        g.add_edge(n0, n1);
        g.add_edge(n2, n1);

        using node_id = typename decltype(g)::node_id;
        auto nodes = std::vector<node_id>();

        SUBCASE("dfs_preorder starting on a given node")
        {
            dfs_preorder_nodes_for_each_id(g, [&nodes](node_id n) { nodes.push_back(n); }, n0);
            CHECK_EQ(nodes, std::vector<node_id>{ n0, n1 });
        }

        SUBCASE("dfs_preorder on all nodes")
        {
            REQUIRE(g.has_node(n0));
            REQUIRE(g.has_node(n1));
            REQUIRE(g.has_node(n2));
            dfs_preorder_nodes_for_each_id(g, [&nodes](node_id n) { nodes.push_back(n); });
            CHECK(is_node_id_permutation(g, nodes.cbegin(), nodes.cend()));
            CHECK_EQ(nodes, std::vector<node_id>{ n0, n1, n2 });
        }

        SUBCASE("dfs_postorder starting on a given node")
        {
            dfs_postorder_nodes_for_each_id(g, [&nodes](node_id n) { nodes.push_back(n); }, n0);
            CHECK_EQ(nodes, std::vector<node_id>{ n1, n0 });
        }

        SUBCASE("dfs_postorder on all nodes")
        {
            dfs_postorder_nodes_for_each_id(g, [&nodes](node_id n) { nodes.push_back(n); });
            CHECK(is_node_id_permutation(g, nodes.cbegin(), nodes.cend()));
            CHECK_EQ(nodes, std::vector<node_id>{ n1, n0, n2 });
        }
    }

    TEST_CASE("Topological sort")
    {
        // How to dress yourself in the morning
        // Introduction to Algorithms, Cormen et al.
        auto g = DiGraph<std::string>();
        const auto undershorts = g.add_node("undershorts");
        const auto pants = g.add_node("pants");
        const auto belt = g.add_node("belt");
        const auto shirt = g.add_node("shirt");
        const auto tie = g.add_node("tie");
        const auto jacket = g.add_node("jacket");
        const auto socks = g.add_node("socks");
        const auto shoes = g.add_node("shoes");
        /* const auto watch = */ g.add_node("watch");
        g.add_edge(undershorts, pants);
        g.add_edge(undershorts, shoes);
        g.add_edge(socks, shoes);
        g.add_edge(pants, shoes);
        g.add_edge(pants, belt);
        g.add_edge(belt, jacket);
        g.add_edge(shirt, belt);
        g.add_edge(shirt, tie);
        g.add_edge(tie, jacket);

        using node_id = typename decltype(g)::node_id;
        auto sorted = std::vector<node_id>();
        topological_sort_for_each_node_id(g, [&sorted](node_id n) { sorted.push_back(n); });

        CHECK(is_node_id_permutation(g, sorted.cbegin(), sorted.cend()));

        g.for_each_edge_id(
            [&](node_id from, node_id to)
            {
                CAPTURE(std::pair(g.node(from), g.node(to)));
                const auto from_pos = std::find(sorted.cbegin(), sorted.cend(), from);
                // Must be true given the permuation assumption
                REQUIRE_LT(from_pos, sorted.cend());
                const auto to_pos = std::find(sorted.cbegin(), sorted.cend(), to);
                // Must be true given the permuation assumption
                REQUIRE_LT(to_pos, sorted.cend());
                // The topological sort property
                CHECK_LT(from_pos, to_pos);
            }
        );
    }

    TEST_CASE("is_reachable")
    {
        auto graph = build_graph();
        CHECK(is_reachable(graph, 0, 6));
        CHECK_FALSE(is_reachable(graph, 6, 0));
    }
}
