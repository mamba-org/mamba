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

#include "property_graph.hpp"
#include "problems_graph.hpp"
#include "util.hpp"

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
        MProblemsExplainer(const problems_graph& problemGraph, 
            std::unordered_map<node_id, std::unordered_set<node_id>> m_group_solvables_to_conflicts);
        
        std::string explain();

    private:
        MPropertyGraph<MGroupNode, MGroupEdgeInfo> m_graph;
        std::unordered_map<node_id, std::unordered_set<node_id>> m_group_solvables_to_conflicts;
        std::unordered_map<std::string, std::unordered_map<size_t, std::unordered_set<std::string>>> conflicts_to_roots;

        std::string explain_problem(const std::pair<MGroupNode, MGroupEdgeInfo>& node) const;
        std::string explain(const std::vector<std::pair<MGroupNode, MGroupEdgeInfo>>& edges) const;
        std::string explain(const std::tuple<MGroupNode, MGroupEdgeInfo, MGroupEdgeInfo>& node_to_edge_to_req) const;
    };

    inline 
    MProblemsExplainer::MProblemsExplainer(const problems_graph& graph, 
    std::unordered_map<node_id, std::unordered_set<node_id>> group_solvables_to_conflicts) 
        : m_graph(graph)
        , m_group_solvables_to_conflicts(group_solvables_to_conflicts) { }

    inline std::string MProblemsExplainer::explain()
    {
        std::unordered_map<node_id, std::vector<std::pair<node_id, MGroupEdgeInfo>>> path = m_graph.get_parents_to_leaves();
        //group_id => group_id: m_graph.group_solvables_to_conflicts;
        std::stringstream sstr;

        std::unordered_map<std::string, std::vector<std::pair<MGroupNode, MGroupEdgeInfo>>> bluf_problems_packages;
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
                auto itc = m_group_solvables_to_conflicts.find(it->first);
                if (itc != m_group_solvables_to_conflicts.end())
                {
                    auto value = hash(itc->second);
                    //TODO check again here the hash is correct
                    conflicts_to_roots[conflict_name][value].insert(
                        root_edge_info.m_deps.begin(), 
                        root_edge_info.m_deps.end()
                    );
                } 
                else 
                {
                    bluf_problems_packages[conflict_name].emplace_back(conflict_node, root_edge_info);
                }
            }
        }

        for (const auto& entry : conflicts_to_roots)
        {
            const std::string conflict_name = entry.first;
            sstr << "Requested packages ";
            for (const auto& hash_to_deps : entry.second)
            {
                sstr << "["  << join(hash_to_deps.second) << ",] ";
            }
            sstr << std::endl;
            sstr << "\tare incompatible because they depend on different versions of "<< conflict_name << std::endl;
            for (const auto& entry : conflict_to_root_info[conflict_name])
            {
                std::string str_node = explain(entry);
                sstr << "\t\t" << str_node << std::endl;
            }
        }

        for (const auto& entry: bluf_problems_packages)
        {
            sstr << "Requested packages " 
                << explain(entry.second) << std::endl
                << "\tcannot be installed because they depend on " << std::endl;
            for (const auto& node_edge_problem : entry.second)
            {
                sstr << "\t\t " << explain_problem(node_edge_problem);
            }
        }
        return sstr.str();
    }

    inline std::string  MProblemsExplainer::explain_problem(const std::pair<MGroupNode, MGroupEdgeInfo>& node_edge) const
    {   
        auto& node = node_edge.first;
        std::stringstream ss;
        if (!node.m_problem_type.has_value())
        {
            ss << node << " which is problematic";
            return ss.str();
        }
        switch (node.m_problem_type.value())
        {   
            case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                ss << node << " which can't be found in the configured channels";
            case SOLVER_RULE_BEST:
                ss << node << " that can not be installed";
            case SOLVER_RULE_BLACK:
                ss << node << " that can only be installed by a direct request";
            case SOLVER_RULE_DISTUPGRADE:
                ss << node << " that does not belong to a distupgrade repository";
            case SOLVER_RULE_INFARCH:
                ss << node << " that has an inferior architecture";
            case SOLVER_RULE_UPDATE:
            case SOLVER_RULE_PKG_NOT_INSTALLABLE:
                ss << node << " that is disabled/has incompatible arch/is not installable";
            case SOLVER_RULE_STRICT_REPO_PRIORITY:
                ss << node << " that is excluded by strict repo priority";
            default:
                LOG_WARNING << "Shouldn't be here";
                ss << node << " which is problematic";
                break;
        }
        return ss.str();
        
    }

    inline std::string MProblemsExplainer::explain(const std::vector<std::pair<MGroupNode, MGroupEdgeInfo>>& requested_packages) const
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
