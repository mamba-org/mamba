#include <gtest/gtest.h>

#include "mamba/core/problems_explainer.hpp"
#include "mamba/core/problems_graph.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/solver.hpp"

namespace mamba
{
    using node_id = MProblemsGraphs::node_id;
    size_t expect_same_union_only(UnionFind<node_id> u, std::vector<size_t> elements)
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
        MNode root;
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
        MNode nonExistent("non-existent >= 2.0.0", SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP);
        std::vector<MNode> nodes{ root,  pyicons1, pyicons2, intl1,  intl2,  intl3,
                                  intl5, menu14,   menu20,   menu21, menu22, nonExistent };

        MProblemsGraphs g;
        std::unordered_map<MNode, MProblemsGraphs::node_id, MNode::HashFunction> visited;
        for (const auto& node : nodes)
        {
            node_id id = g.get_or_create_node(node);
            visited[node] = id;
            std::cerr << "\nInfo " << node.get_name() << " " << visited[node] << " == " << id
                      << std::endl;
        }

        g.add_conflict_edge(root, menu14, "menu*");
        g.add_conflict_edge(root, menu20, "menu*");
        g.add_conflict_edge(root, menu21, "menu*");
        g.add_conflict_edge(root, menu22, "menu*");
        g.add_conflict_edge(root, pyicons2, "pyicons 2.*");
        g.add_conflict_edge(root, intl5, "intl 5.*");

        g.add_conflict_edge(menu14, intl1, "intl 1.*");
        g.add_conflict_edge(menu14, pyicons1, "pyicons 1.*");
        g.add_conflict_edge(menu14, nonExistent, "non-existent >=2.0.0");

        std::vector<MNode> menu2_same_kids{ menu20, menu21, menu22 };
        for (const auto& menu : menu2_same_kids)
        {
            g.add_conflict_edge(menu, intl2, "intl 2.*");
            g.add_conflict_edge(menu, intl3, "intl 3.*");
            g.add_conflict_edge(menu, pyicons1, "pyicons 1.*");
        }

        g.add_solvables_to_conflicts(visited[pyicons1], visited[pyicons2]);
        g.add_solvables_to_conflicts(visited[intl1], visited[intl5]);
        g.add_solvables_to_conflicts(visited[intl2], visited[intl5]);
        g.add_solvables_to_conflicts(visited[intl3], visited[intl5]);

        g.create_unions();
        MProblemsGraphs::node_id union1 = expect_same_union_only(
            g.m_union, std::vector{ visited[menu20], visited[menu21], visited[menu22] });
        MProblemsGraphs::node_id union2
            = expect_same_union_only(g.m_union, std::vector{ visited[menu14] });
        MProblemsGraphs::node_id union3
            = expect_same_union_only(g.m_union, std::vector{ visited[intl2], visited[intl3] });
        EXPECT_TRUE(union1 != union2 && union1 != union3 && union2 != union3);

        auto merged_graph = g.create_merged_graph();
        auto groups = g.get_groups_conflicts();

        MProblemsExplainer explainer(merged_graph, groups);
        std::cerr << explainer.explain() << std::endl;
    }
}
