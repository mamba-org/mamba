#include <gtest/gtest.h>

#include <list>
#include <optional>

#include "mamba/core/problems_explainer.hpp"
#include "mamba/core/pool.hpp"

namespace mamba
{

    MProblemsExplainer build_explainer()
    {
        /*
        SolverRuleinfo.SOLVER_RULE_PKG_SAME_NAME pyicons-1.0.0- ( 21 ) None ( 0 ) pyicons-2.0.0- ( 20 )
        SolverRuleinfo.SOLVER_RULE_JOB menu-1.4.0- ( 6 ) pyicons 2.* ( -2147483607 ) None ( 259 )
        SolverRuleinfo.SOLVER_RULE_JOB None ( 0 ) pyicons 1.* ( -2147483609 ) None ( 259 )
        */
        MNode pyicons1 = MNode(PackageInfo("pyicons", "1.0.0", "abcde", 0));
        MNode pyicons2 = MNode(PackageInfo("pyicons", "2.0.0", "abcde", 0));
        MNode menu = MNode(PackageInfo("menu", "1.4.0", "abcde", 0));
        std::vector<MNode> nodes
        {
            pyicons1,
            pyicons2,
            menu,
        };
        std::unordered_map<MNode, MProblemsExplainer::node_id, MNode::HashFunction> visited;
        MProblemsExplainer e;
        for (const auto& node : nodes)
        {
            visited[node] = e.get_or_create_node(node, visited);
        }

        e.add_solvables_to_conflicts(visited[pyicons1], visited[pyicons2]);
        return e;
    }

    TEST(problems_explainer, test_initial_graph_creation)
    {
        MProblemsExplainer e = build_explainer();
        EXPECT_TRUE(e.explain_conflicts() == "dssnaas");
    }


    /*TEST(problems_explainer, test_merged_graph_creation)
    {
    }

    TEST(problems_explainer, problems_explainer)
    {
    }*/

}