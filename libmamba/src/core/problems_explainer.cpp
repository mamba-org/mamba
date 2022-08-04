// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include "mamba/core/problems_explainer.hpp"

#include <tuple>

namespace mamba
{
    MProblemsExplainer::MProblemsExplainer(const MPropertyGraph<MGroupNode, MGroupEdgeInfo>& graph) : m_graph(graph) { }

    std::string MProblemsExplainer::explain() const
    {
        std::unordered_map<node_id, std::vector<std::pair<node_id, MGroupEdgeInfo>>> path = m_graph.get_parents_to_leaves();
        std::stringstream sstr;

        std::unordered_map<std::string, std::vector<std::pair<MGroupNode, MGroupEdgeInfo>>> bluf;
        std::unordered_map<std::string, std::vector<std::tuple<MGroupNode, MGroupEdgeInfo, MGroupEdgeInfo>>> conflict_to_root_info;
        for (const auto& entry : path) 
        {

            MGroupNode root_node = m_graph.get_node(entry.first);
            // currently the vector only contains the root as a first entry & all the leaves
            const auto& root_info = entry.second[0];
            const auto& root_edge_info = root_info.second;
            for (auto it = entry.second.begin() + 1; it != entry.second.end(); it++)
            {
                MGroupNode conflict_node = m_graph.get_node(it->first);
                //TODO consider the invalid conflict_node
                conflict_to_root_info[conflict_node.m_pkg_name].push_back(std::make_tuple(root_node, root_edge_info, it->second));
                bluf[conflict_node.m_pkg_name].push_back(std::make_pair(root_node, root_edge_info));
            }
        }

        for (const auto& entry : bluf)
        {
            sstr << "Requested packages "
                << explain(entry.second)
                << "are incompatible because they depend on different versions of package " << entry.first;
            for (const auto& entry : conflict_to_root_info[entry.first])
            {
                std::string str_node = explain(entry);
                sstr << "\t " << str_node;
            }
        }
        return sstr.str();
    }


    std::string MProblemsExplainer::explain(const std::vector<std::pair<MGroupNode, MGroupEdgeInfo>>& nodes_to_edge) const
    {
        std::stringstream sstr;
        for (const auto& node_to_edge: nodes_to_edge)
        {
            sstr << join(node_to_edge.second.m_deps);
        }
        return sstr.str();
    }

    std::string MProblemsExplainer::explain(const std::tuple<MGroupNode, MGroupEdgeInfo, MGroupEdgeInfo>& node_to_edge_to_req) const
    {
        std::stringstream sstr;
        MGroupNode group_node = std::get<0>(node_to_edge_to_req);
        MGroupEdgeInfo group_node_edge = std::get<1>(node_to_edge_to_req);
        MGroupEdgeInfo conflict_edge = std::get<2>(node_to_edge_to_req);
        sstr << join(group_node_edge.m_deps) << " " << join(group_node.m_pkg_versions) << " depend on " << join(conflict_edge.m_deps);
        return sstr.str();
    }

}
