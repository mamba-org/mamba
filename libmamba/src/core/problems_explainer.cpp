// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include "mamba/core/problems_explainer.hpp"

#include <tuple>

namespace mamba
{
    MProblemsExplainer::MProblemsExplainer(const MProblemsExplainer::graph& g)
        : m_problems_graph(g)
    {
    }

    std::string MProblemsExplainer::explain()
    {
        std::vector<node_id> roots = m_problems_graph.get_roots();
        // TODO we have 1 root from installed
        //& we have multiple roots (in case of UPDATE rules)
        node_path roots_to_paths;
        for (const auto& root : roots)
        {
            node_path nodes_to_paths = m_problems_graph.get_paths_from(root);
            for (const auto& node_to_path : nodes_to_paths)
            {
                roots_to_paths[node_to_path.first].insert(roots_to_paths[node_to_path.first].end(),
                                                          node_to_path.second.begin(),
                                                          node_to_path.second.end());
            }
        }

        std::unordered_map<std::string, std::vector<node_edge>> bluf_problems_packages;
        std::unordered_map<std::string, std::unordered_map<std::string, std::vector<node_edge>>>
            conflict_to_root_info;

        for (const auto& entry : roots_to_paths)
        {
            MGroupNode root_node = m_problems_graph.get_node(entry.first);
            // currently the vector only contains the root as a first entry & all the leaves
            const auto& root_info = entry.second[0];
            const auto& root_edge_info = root_info.second;
            std::cerr << "Root node" << root_info.first << " " << root_info.second << std::endl;
            for (auto it = entry.second.begin() + 1; it != entry.second.end(); it++)
            {
                MGroupNode conflict_node = m_problems_graph.get_node(it->first);
                auto conflict_name = conflict_node.get_name();
                conflict_to_root_info[conflict_name][join(it->second.m_deps, ", ")].push_back(
                    std::make_pair(root_node, root_edge_info));
                bluf_problems_packages[conflict_name].push_back(
                    std::make_pair(conflict_node, root_edge_info));
            }
        }

        std::stringstream sstr;
        for (const auto& entry : bluf_problems_packages)
        {
            auto conflict_name = entry.first;
            auto conflict_node = entry.second[0].first;  // only the first required
            std::vector<node_edge> conflict_to_root_deps = entry.second;
            std::unordered_set<std::string> requested;
            for (const auto& conflict_to_dep : conflict_to_root_deps)
            {
                requested.insert(conflict_to_dep.second.m_deps.begin(),
                                 conflict_to_dep.second.m_deps.end());
            }
            sstr << "Requested packages " << explain(requested) << " cannot be installed because ";
            if (conflict_node.is_conflict())
            {
                sstr << "of different versions of " << conflict_name << std::endl;
                std::unordered_set<std::string> conflicts;
                for (const auto& conflict_deps_to_root_info : conflict_to_root_info[conflict_name])
                {
                    std::unordered_map<std::string, std::unordered_set<std::string>>
                        roots_to_versions;
                    for (const auto& root_info : conflict_deps_to_root_info.second)
                    {
                        if (conflict_node.get_name() == root_info.first.get_name())
                        {
                            continue;
                        }
                        roots_to_versions[join(root_info.second.m_deps)].insert(
                            join(root_info.first.m_pkg_versions));
                    }
                    for (const auto& root_to_version : roots_to_versions)
                    {
                        conflicts.insert(root_to_version.first + " versions ["
                                         + join(root_to_version.second) + "]" + "\n\tdepend on "
                                         + conflict_deps_to_root_info.first);
                    }
                }
                sstr << join(conflicts, "\n") << std::endl;
            }
            else
            {
                sstr << "they depend on ";
                sstr << "\n\t" << explain_problem(conflict_node) << std::endl;
            }
        }
        return sstr.str();
    }

    std::string MProblemsExplainer::explain_problem(const MGroupNode& node) const
    {
        std::stringstream ss;
        if (!node.m_problem_type)
        {
            ss << node << " which is problematic";
            return ss.str();
        }
        std::string package_name = node.get_name();
        switch (node.m_problem_type.value())
        {
            case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
                ss << package_name << " which does not exist in the configured channels";
                break;
            case SOLVER_RULE_BEST:
                ss << package_name << " that can not be installed";
                break;
            case SOLVER_RULE_BLACK:
                ss << package_name << " that can only be installed by a direct request";
                break;
            case SOLVER_RULE_DISTUPGRADE:
                ss << package_name << " that does not belong to a distupgrade repository";
                break;
            case SOLVER_RULE_INFARCH:
                ss << package_name << " that has an inferior architecture";
                break;
            case SOLVER_RULE_UPDATE:
            case SOLVER_RULE_PKG_NOT_INSTALLABLE:
                ss << package_name << " that is disabled/has incompatible arch/is not installable";
                break;
            case SOLVER_RULE_STRICT_REPO_PRIORITY:
                ss << package_name << " that is excluded by strict repo priority";
                break;
            default:
                LOG_WARNING << "Default message for problem type" << node.m_problem_type.value()
                            << " " << node << std::endl;
                ss << package_name << " which is problematic";
                break;
        }
        ss << std::endl;
        return ss.str();
    }

    std::string MProblemsExplainer::explain(
        const std::unordered_set<std::string>& requested_packages) const
    {
        return join(requested_packages, ",");
    }

    std::string MProblemsExplainer::explain(const node_edge& node_to_edge) const
    {
        std::stringstream sstr;
        MGroupNode group_node = node_to_edge.first;
        MGroupEdgeInfo group_node_edge = node_to_edge.second;
        sstr << group_node_edge << " versions: [" << join(group_node.m_pkg_versions, ", ") << "]"
             << std::endl;
        return sstr.str();
    }
}
