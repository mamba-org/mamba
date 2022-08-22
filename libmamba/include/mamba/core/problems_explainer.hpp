// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_EXPLAINER_HPP
#define MAMBA_PROBLEMS_EXPLAINER_HPP

#include <string>
#include <utility>
#include <tuple>
#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <numeric>

#include "problems_graph.hpp"
#include "property_graph.hpp"
#include "util.hpp"
#include "pool.hpp"

extern "C"
{
#include "solv/solver.h"
}

namespace mamba
{
    class MProblemsExplainer
    {
    public:
        using graph = MPropertyGraph<MGroupNode, MGroupEdgeInfo>;
        using node_id = MPropertyGraph<MGroupNode, MGroupEdgeInfo>::node_id;
        using node_path = MPropertyGraph<MGroupNode, MGroupEdgeInfo>::node_path;
        using node_edge = std::pair<MGroupNode, MGroupEdgeInfo>;
        using node_edge_edge = std::tuple<MGroupNode, MGroupEdgeInfo, MGroupEdgeInfo>;

        MProblemsExplainer(const graph& g);
        std::string explain();

    private:
        graph m_problems_graph;

        std::string explain_problem(const MGroupNode& node) const;
        std::string explain(const std::unordered_set<std::string>& deps) const;
        std::string explain(const node_edge& node_to_edge) const;
    };
}  // namespace mamba

#endif  // MAMBA_PROBLEMS_EXPLAINER_HPP
