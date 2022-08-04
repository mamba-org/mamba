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

#include "solver.hpp"
#include "property_graph.hpp"
#include "problems_graph.hpp"
#include "mamba/core/package_info.hpp"
#include "pool.hpp"

extern "C"
{
#include "solv/queue.h"
#include "solv/solver.h"
}

namespace mamba
{
    class MGroupNode;
    class MGroupEdgeInfo;
    
    class MProblemsExplainer
    {
    public:
        using node_id = MPropertyGraph<MGroupNode, MGroupEdgeInfo>::node_id;
        MProblemsExplainer(const MPropertyGraph<MGroupNode, MGroupEdgeInfo>& problemGraph);
        
        std::string explain_conflicts() const;

    private:
        MPropertyGraph<MGroupNode, MGroupEdgeInfo> m_graph;

        std::string explain() const;
        std::string explain(const std::vector<std::pair<MGroupNode, MGroupEdgeInfo>>& nodes_to_edge) const;
        std::string explain(const std::tuple<MGroupNode, MGroupEdgeInfo, MGroupEdgeInfo>& node_to_edge_to_req) const;
    };

    inline std::string join(std::vector<std::string> const &vec)
    {
        return "[" +  std::accumulate(vec.begin(), vec.end(), std::string(",")) + "]";
    }
}  // namespace mamba

#endif  // MAMBA_PROBLEMS_EXPLAINER_HPP
