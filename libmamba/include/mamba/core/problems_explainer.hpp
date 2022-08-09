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
        using adj_list = std::unordered_map<node_id, std::unordered_set<node_id>>;

        MProblemsExplainer(const graph& g, const adj_list& adj);
        std::string explain();

    private:
        graph m_problems_graph;
        adj_list m_conflicts_adj_list;

        std::string explain_problem(const MGroupNode& node) const;
        std::string explain(const std::vector<node_edge>& edges) const;
        std::string explain(const node_edge_edge& node_to_edge_to_req) const;
    };

    inline MProblemsExplainer::MProblemsExplainer(const MProblemsExplainer::graph& g,
                                                  const MProblemsExplainer::adj_list& adj)
        : m_problems_graph(g)
        , m_conflicts_adj_list(adj)
    {
    }

    inline std::string MProblemsExplainer::explain()
    {
        node_path path = m_problems_graph.get_parents_to_leaves();

        std::unordered_map<std::string, std::vector<node_edge>> bluf_problems_packages;
        std::unordered_map<std::string, std::vector<node_edge_edge>> conflict_to_root_info;
        std::unordered_map<std::string, std::unordered_map<size_t, std::unordered_set<std::string>>>
            conflicts_to_roots;
        for (const auto& entry : path)
        {
            MGroupNode root_node = m_problems_graph.get_node(entry.first);
            // currently the vector only contains the root as a first entry & all the leaves
            const auto& root_info = entry.second[0];
            const auto& root_edge_info = root_info.second;
            for (auto it = entry.second.begin() + 1; it != entry.second.end(); it++)
            {
                MGroupNode conflict_node = m_problems_graph.get_node(it->first);
                auto conflict_name = conflict_node.get_name();
                conflict_to_root_info[conflict_name].push_back(
                    std::make_tuple(root_node, root_edge_info, it->second));
                auto itc = m_conflicts_adj_list.find(it->first);
                if (itc != m_conflicts_adj_list.end())
                {
                    auto value = hash(itc->second);
                    // TODO check again here the hash is correct
                    conflicts_to_roots[conflict_name][value].insert(root_edge_info.m_deps.begin(),
                                                                    root_edge_info.m_deps.end());
                }
                else
                {
                    bluf_problems_packages[conflict_name].emplace_back(conflict_node,
                                                                       root_edge_info);
                }
            }
        }

        std::stringstream sstr;
        for (const auto& entry : conflicts_to_roots)
        {
            const std::string conflict_name = entry.first;
            sstr << "Requested packages ";
            for (const auto& hash_to_deps : entry.second)
            {
                sstr << "[" << join(hash_to_deps.second) << ",] ";
            }
            sstr << std::endl;
            sstr << "\tare incompatible because they depend on different versions of "
                 << conflict_name << std::endl;
            for (const auto& entry : conflict_to_root_info[conflict_name])
            {
                std::string str_node = explain(entry);
                sstr << "\t\t" << str_node << std::endl;
            }
        }

        for (const auto& entry : bluf_problems_packages)
        {
            sstr << "Requested packages " << explain(entry.second) << std::endl
                 << "\tcannot be installed because they depend on " << std::endl;
            for (const auto& node_edge_problem : entry.second)
            {
                sstr << "\t\t " << explain_problem(node_edge_problem.first);
            }
        }
        return sstr.str();
    }

    inline std::string MProblemsExplainer::explain_problem(const MGroupNode& node) const
    {
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

    inline std::string MProblemsExplainer::explain(
        const std::vector<node_edge>& requested_packages) const
    {
        std::stringstream sstr;
        for (const auto& requested_package : requested_packages)
        {
            sstr << requested_package.second;
        }
        return sstr.str();
    }

    inline std::string MProblemsExplainer::explain(const node_edge_edge& node_to_edge_to_req) const
    {
        std::stringstream sstr;
        MGroupNode group_node = std::get<0>(node_to_edge_to_req);
        MGroupEdgeInfo group_node_edge = std::get<1>(node_to_edge_to_req);
        MGroupEdgeInfo conflict_edge = std::get<2>(node_to_edge_to_req);
        sstr << group_node_edge << " versions: [" << join(group_node.m_pkg_versions)
             << "] depend on " << conflict_edge;
        return sstr.str();
    }
}  // namespace mamba

#endif  // MAMBA_PROBLEMS_EXPLAINER_HPP
