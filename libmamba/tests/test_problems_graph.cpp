#include <gtest/gtest.h>

#include "mamba/core/problems_graph.hpp"
#include "mamba/core/package_info.hpp"

namespace mamba
{
    using node_id = MProblemsGraphs::node_id;
    size_t expect_same_union_only(Union<node_id> u, std::vector<size_t> elements)
    {
        size_t first_root = u.root(elements[0]);
        for (auto it = elements.begin() + 1; it != elements.end(); ++it)
        {
            EXPECT_TRUE(first_root == u.root(*it));
        }
        return first_root;
    }

    TEST(problems_graph, test_creating_merged_graph)
    {
        MNode pyicons1(PackageInfo("pyicons", "1.0.0", "abcde", 0));
        MNode pyicons2(PackageInfo("pyicons", "2.0.0", "abcde", 0));
        MNode intl1(PackageInfo("intl", "1.0.0", "abcde", 0));
        MNode intl2(PackageInfo("intl", "2.0.0", "abcde", 0));
        MNode intl3(PackageInfo("intl", "3.0.0", "abcde", 0));
        MNode intl5(PackageInfo("intl", "5.0.0", "abcde", 0));
        MNode menu14(PackageInfo("menu", "1.4.0", "abcde", 0));
        MNode menu20(PackageInfo("menu", "2.0.0", "abcde", 0));
        MNode menu21(PackageInfo("menu", "2.0.1", "abcde", 0));
        MNode menu22(PackageInfo("menu", "2.0.2", "abcde", 0));
        std::vector<MNode> nodes
        {
            pyicons1,
            pyicons2,
            intl1,
            intl2,
            intl3, 
            menu14,
            menu20,
            menu21,
            menu22
        };
    
        MProblemsGraphs g;
        std::unordered_map<MNode, MProblemsGraphs::node_id, MNode::HashFunction> visited;
        for (const auto& node : nodes)
        {
            node_id id = g.get_or_create_node(node);
            visited[node] = id;
            std::cerr << "\nInfo " << node.m_package_info.value().name << " " << visited[node] << " == " << id << std::endl;
        }

        g.add_conflict_edge(menu14, intl1, "intl 1.*");
        g.add_conflict_edge(menu14, pyicons1, "pyicons 1.*");
        g.add_conflict_edge(menu20, intl2, "intl 2.*");
        g.add_conflict_edge(menu20, intl3, "intl 3.*");
        g.add_conflict_edge(menu20, pyicons1, "pyicons 1.*");
        g.add_conflict_edge(menu21, intl2, "intl 2.*");
        g.add_conflict_edge(menu21, intl3, "intl 3.*");
        g.add_conflict_edge(menu21, pyicons1, "pyicons 1.*");
        g.add_conflict_edge(menu22, intl2, "intl 2.*");
        g.add_conflict_edge(menu22, intl3, "intl 3.*");
        g.add_conflict_edge(menu22, pyicons1, "pyicons 1.*");

        g.add_solvables_to_conflicts(visited[pyicons1], visited[pyicons2]);
        g.add_solvables_to_conflicts(visited[intl1], visited[intl5]);
        g.add_solvables_to_conflicts(visited[intl2], visited[intl5]);
        g.add_solvables_to_conflicts(visited[intl3], visited[intl5]);

        g.create_unions();
        MProblemsGraphs::node_id union1 = expect_same_union_only(g.m_union, std::vector{visited[menu20], visited[menu21], visited[menu22]});
        MProblemsGraphs::node_id union2 = expect_same_union_only(g.m_union, std::vector{visited[menu14]});
        MProblemsGraphs::node_id union3 = expect_same_union_only(g.m_union, std::vector{visited[intl2], visited[intl3]});
        EXPECT_TRUE(union1 != union2 && union1 != union3 && union2 != union3);

        g.create_merged_graph();
        
        std::vector<std::vector<std::pair<node_id, MGroupEdgeInfo>>> adj_list = g.m_merged_conflict_graph.get_adj_list();
        for(node_id i = 0; i < adj_list.size()
        ; ++i)
        {   
            MGroupNode from_node = g.m_merged_conflict_graph.get_node(i);
            std::cerr << "\nfrom " << from_node;
            for(size_t j = 0; j < adj_list[i].size()
            ; ++j)
            {
                std::cerr << "\t " << g.m_merged_conflict_graph.get_node(adj_list[i][j].first)
                     << " " << adj_list[i][j].second << std::endl;
            }
        }
    }
}
