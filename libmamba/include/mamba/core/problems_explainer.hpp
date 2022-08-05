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
#include <numeric>

#include "property_graph.hpp"
#include "problems_graph.hpp"

namespace mamba
{
    class MGroupNode;
    class MGroupEdgeInfo;
    
    class MProblemsExplainer
    {
    public:
        using problems_graph = MPropertyGraph<MGroupNode, MGroupEdgeInfo>;
        using node_id = MPropertyGraph<MGroupNode, MGroupEdgeInfo>::node_id;
        using pkg_dep = std::pair<MGroupNode, MGroupEdgeInfo>;
        MProblemsExplainer(const problems_graph& problemGraph);
        
        std::string explain() const;

    private:
        MPropertyGraph<MGroupNode, MGroupEdgeInfo> m_graph;

        std::string explain(const std::vector<std::pair<MGroupNode, MGroupEdgeInfo>>& nodes_to_edge) const;
        std::string explain(const std::tuple<MGroupNode, MGroupEdgeInfo, MGroupEdgeInfo>& node_to_edge_to_req) const;
    };

    inline 
    MProblemsExplainer::MProblemsExplainer(const problems_graph& graph) : m_graph(graph) { }

    inline std::string MProblemsExplainer::explain() const
    {
        std::unordered_map<node_id, std::vector<std::pair<node_id, MGroupEdgeInfo>>> path = m_graph.get_parents_to_leaves();
        std::stringstream sstr;

        std::unordered_map<std::string, std::vector<pkg_dep>> bluf;
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
                auto conflict_name = conflict_node.get_name();
                conflict_to_root_info[conflict_name].push_back(std::make_tuple(root_node, root_edge_info, it->second));
                bluf[conflict_name].push_back(std::make_pair(root_node, root_edge_info));
            }
        }

        for (const auto& entry : bluf)
        {
            sstr << "Requested packages " 
                << explain(entry.second) << std::endl
                << "\tare incompatible because they depend on different versions of package " << entry.first << std::endl;
            for (const auto& entry : conflict_to_root_info[entry.first])
            {
                std::string str_node = explain(entry);
                sstr << "\t\t" << str_node << std::endl;
            }
        }
        return sstr.str();
    }

    inline std::string join(std::unordered_set<std::string> const &strings)
    {
        std::stringstream ss;
        std::copy(strings.begin(), strings.end(),
            std::ostream_iterator<std::string>(ss, ", "));
        return ss.str();
    }


    inline std::string MProblemsExplainer::explain(const std::vector<pkg_dep>& requested_packages) const
    {
        std::stringstream sstr;
        for (const auto& requested_package: requested_packages)
        {
            sstr << requested_package.second;
        }
        return sstr.str();
    }

    inline std::string MProblemsExplainer::explain(const std::tuple<MGroupNode, MGroupEdgeInfo, MGroupEdgeInfo>& node_to_edge_to_req) const
    {
        std::stringstream sstr;
        MGroupNode group_node = std::get<0>(node_to_edge_to_req);
        MGroupEdgeInfo group_node_edge = std::get<1>(node_to_edge_to_req);
        MGroupEdgeInfo conflict_edge = std::get<2>(node_to_edge_to_req);
        sstr << group_node_edge << " versions: [" << join(group_node.m_pkg_versions) << "] depend on " << conflict_edge;
        return sstr.str();
    }
}  // namespace mamba

#endif  // MAMBA_PROBLEMS_EXPLAINER_HPP
