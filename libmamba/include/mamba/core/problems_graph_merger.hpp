// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_GRAPH_MERGER_HPP
#define MAMBA_PROBLEMS_GRAPH_MERGER_HPP

#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <unordered_map>
#include <tuple>
#include <functional>
#include <iostream>

#include "mamba/core/union_find.hpp"
#include "mamba/core/problems_graph.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{

    class MProblemsGraphMerger
    {
    public:
        using initial_graph = MProblemsGraph<MNode, MEdgeInfo>;
        using merged_graph = MProblemsGraph<MGroupNode, MGroupEdgeInfo>;
        using node_id = initial_graph::node_id;
        using group_node_id = merged_graph::node_id;
        using node_id_to_group_id = std::unordered_map<node_id, group_node_id>;

        MProblemsGraphMerger(const initial_graph& graph);

        const merged_graph& create_merged_graph();

    private:
        UnionFind<node_id> m_union;
        initial_graph m_initial_graph;
        merged_graph m_merged_graph;

        void create_unions();
        node_id_to_group_id create_merged_nodes();
    };
}

#endif  // MAMBA_PROBLEMS_GRAPH_MERGER_HPP