// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include "mamba/core/problems_explainer.hpp"

namespace mamba
{

    MNode::MNode(PackageInfo package_info) : m_package_info(package_info), m_dep(std::nullopt) { }
    
    MNode::MNode(std::string dep) : m_package_info(std::nullopt), m_dep(dep) { }

    bool MNode::operator== (const MNode& other) const 
    { 
        if (m_package_info.has_value() && other.m_package_info.has_value())
        {
            return m_package_info.value() == other.m_package_info.value();
        }
        if (m_dep.has_value() && other.m_dep.has_value()) {
            return m_dep.value() == other.m_dep.value();
        }
        return false;
    }

    bool MNode::is_invalid_dep() 
    {
        return !m_package_info.has_value() && m_dep.has_value();
    }

    void MGroupNode::add(const MNode& node) 
    {
        //TODO throw if wanted to combine deps ? or actually combine the deps ? 
        if (node.m_package_info.has_value())
        {
            m_pkg_versions.push_back(node.m_package_info.value().version);
        }
    }

    MGroupNode::MGroupNode(const MNode& node)
    {
        if (node.m_package_info.has_value())
        {
            m_pkg_versions.push_back(node.m_package_info.value().version);
        }
    }
    
    MGroupNode::MGroupNode() { }

    void MGroupEdgeInfo::add(MEdgeInfo edge) 
    {
        m_deps.push_back(edge.m_dep);
    }

   MGroupEdgeInfo::MGroupEdgeInfo(const MEdgeInfo& edge)
    {
        m_deps.push_back(edge.m_dep);
    }

    MProblemsExplainer::MProblemsExplainer(MPool* pool, const std::vector<MSolverProblem>& problems) 
    : m_pool(pool)
    {
        create_initial_graph(problems);
    }

    void MProblemsExplainer::create_initial_graph(const std::vector<MSolverProblem>& problems) {
        std::unordered_map<MNode, node_id, MNode::HashFunction> visited;
        for (const auto& problem : problems) 
        {
            add_problem_to_graph(problem, visited);
        }
    }

    void MProblemsExplainer::add_problem_to_graph(const MSolverProblem& problem, std::unordered_map<MNode, node_id, MNode::HashFunction>& visited) 
    {
        switch (problem.type) 
        {
            case SOLVER_RULE_PKG_REQUIRES:          // source_id -> dep_id -> target_id
            case SOLVER_RULE_PKG_CONSTRAINS:        // source_id -> dep_id -> <conflict - to - be ignored>
            case SOLVER_RULE_JOB:                   //entry point for our solver
            { 
                std::vector<Id> target_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& target_id : target_ids) 
                {
                    PackageInfo target = PackageInfo(pool_id2solvable(*m_pool, target_id));
                    add_conflict_edge(MNode(problem.source().value()), MNode(target), problem.dep().value(), visited);
                }
            }
            case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP: 
            {
                //source_id -> <invalid_dep> -> None
                add_conflict_edge(MNode(problem.source().value()), MNode(problem.dep().value()), problem.dep().value(), visited);
            }
            case SOLVER_RULE_PKG_CONFLICTS:
            case SOLVER_RULE_PKG_SAME_NAME: 
            {
                // source_id -> <ignore> -> target_id
                node_id source_node = get_or_create_node(MNode(problem.source().value()), visited);
                node_id target_node = get_or_create_node(MNode(problem.target().value()), visited);
                solvables_to_conflicts[source_node].insert(target_node);
                solvables_to_conflicts[target_node].insert(source_node);
            }
            case SOLVER_RULE_UNKNOWN:
            case SOLVER_RULE_PKG:
            case SOLVER_RULE_PKG_NOT_INSTALLABLE:
            case SOLVER_RULE_PKG_SELF_CONFLICT:
            case SOLVER_RULE_PKG_OBSOLETES:
            case SOLVER_RULE_PKG_IMPLICIT_OBSOLETES:
            case SOLVER_RULE_PKG_INSTALLED_OBSOLETES:
            case SOLVER_RULE_PKG_RECOMMENDS:
            case SOLVER_RULE_UPDATE:
            case SOLVER_RULE_FEATURE:
            case SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM:
            case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
            case SOLVER_RULE_JOB_UNSUPPORTED:
            case SOLVER_RULE_DISTUPGRADE:
            case SOLVER_RULE_INFARCH:
            case SOLVER_RULE_CHOICE:
            case SOLVER_RULE_LEARNT:
            case SOLVER_RULE_BEST:
            case SOLVER_RULE_YUMOBS:
            case SOLVER_RULE_RECOMMENDS:
            case SOLVER_RULE_BLACK:
            case SOLVER_RULE_STRICT_REPO_PRIORITY:
                LOG_WARNING << "Problem type not implemented: " << problem.type;
        }
    }

    void MProblemsExplainer::add_conflict_edge(MNode from_node, MNode to_node, std::string info, std::unordered_map<MNode, node_id, MNode::HashFunction>& visited) 
    {
        MProblemsExplainer::node_id from_node_id = get_or_create_node(from_node, visited);
        MProblemsExplainer::node_id to_node_id = get_or_create_node(to_node, visited);
        m_initial_conflict_graph.add_edge(from_node_id, to_node_id, info);
    }

    MProblemsExplainer::node_id MProblemsExplainer::get_or_create_node(MNode mnode, std::unordered_map<MNode, node_id, MNode::HashFunction>& visited) 
    {
        auto it = visited.find(mnode);
        if (it == visited.end())
        {
            auto node_id = m_initial_conflict_graph.add_node(mnode);
            visited.insert(std::make_pair(mnode, node_id));
            return node_id;
        }
        return it->second;
    }

    /*std::vector<package_to_rel> MGraph::get_parents(IMNode* node) {
        std::vector<package_to_rel> emtpy_path;
        auto it = m_adj.find(*node);
        if (m_adj.end() == it) {
            throw std::invalid_argument("We shouldn't be here!");
        }
        if (node->root()) {
            throw std::invalid_argument("We shouldn't be here!");
        }
        for (const auto& edge : m_adj[*node]) {
            if (edges->root()) {
                roots.push_back((node->node_id, edge->rel_id));
            } else {
                std::unordered_set<package_to_rel> current_parents = get_parents(neigh_node)
                roots.insert(current_parents.begin(), current_parents.end());
            }
        }
        return roots;
    }*/

    std::string MProblemsExplainer::explain_conflicts() 
    {
        std::vector<std::unordered_set<node_id>> node_to_neigh;
        std::unordered_map<size_t, std::vector<node_id>> hashes_to_nodes;
        for (node_id i = 0; i < m_initial_conflict_graph.get_node_list().size(); ++i) 
        {
            std::vector<std::pair<node_id, MEdgeInfo>> edge_list = m_initial_conflict_graph.get_edge_list(i);
            std::unordered_set<node_id> neighs;
            std::transform(
                edge_list.begin(), 
                edge_list.end(), 
                std::inserter(neighs, neighs.begin()),
                [](auto &kv){ return kv.first;});
            node_to_neigh[i] = neighs;
            hashes_to_nodes[hash_vec(neighs)].push_back(i);
            m_union.add(i);
        }

        // need to also merge the leaves - the childrens will be the conflicts from same_name 
        for (const auto& solvable_to_conflict : solvables_to_conflicts) 
        {
            hashes_to_nodes[hash_vec(solvable_to_conflict.second)].push_back(solvable_to_conflict.first);
            node_to_neigh[solvable_to_conflict.first].insert(solvable_to_conflict.second.begin(), solvable_to_conflict.second.end());
            m_union.add(solvable_to_conflict.first);
        }
        
        // we go through the list of conflicts and make sure that we can merge the information
        for (const auto& hash_to_nodes : hashes_to_nodes) 
        { 
            for (node_id i = 0; i < hash_to_nodes.second.size(); ++i) 
            {
                std::optional<std::string> maybe_package_name_i = get_package_name(i);
                if (!maybe_package_name_i.has_value()) 
                {
                    continue;
                }
                for (node_id j = i+1; j < hash_to_nodes.second.size(); ++j) 
                {
                    std::optional<std::string> maybe_package_name_j = get_package_name(j);
                    if (!maybe_package_name_j.has_value()) 
                    {
                        continue;
                    }
                    if (node_to_neigh[hash_to_nodes.second[i]] == node_to_neigh[hash_to_nodes.second[j]]
                        &&  maybe_package_name_i == maybe_package_name_j)
                        // making sure that we only merge packages with the same names
                    {
                        m_union.connect(i, j);
                    }
                }
            }
        }

        create_merged_graph();

        std::stringstream sstr;
        return sstr.str();

        // combine information from the union trees
        //std::unordered_map<MNodeGroup, std::vector<MEdgeGroup>> new_adj_list;
        //create_graph(new_adj_list);
        //traverse graph from leaves -> get <leaf -> parent> & print
        //get_parents & print
    }

    std::optional<std::string> MProblemsExplainer::get_package_name(node_id id) {
        auto maybe_package_info = m_initial_conflict_graph.get_node(id).m_package_info;
        if (maybe_package_info.has_value()) 
        {
            return maybe_package_info.value().name;
        }
        return std::nullopt;

    }

    std::unordered_map<MProblemsExplainer::node_id, MProblemsExplainer::group_node_id> MProblemsExplainer::create_merged_nodes() {
        std::unordered_map<node_id, group_node_id> node_id_to_group_id;
        std::unordered_map<node_id, node_id> parents = m_union.parent;
        auto key_selector = [](auto pair){return pair.first;};
        std::vector<node_id> keys(parents.size());
        std::transform(parents.begin(), parents.end(), keys.begin(), key_selector);
        // go through each one of them and set the information to its info & its kids 
        for (const auto& id : keys) {
            MNode node = m_initial_conflict_graph.get_node(id);
            node_id parent = m_union.root(id);
            auto it = node_id_to_group_id.find(parent);
            if (it != node_id_to_group_id.end()) 
            {
                //TODO implement this
                m_merged_conflict_graph.update_node(it->second, node);
            } 
            else 
            {
                MGroupNode group_node(node);
                group_node_id new_id = m_merged_conflict_graph.add_node(group_node);
                node_id_to_group_id[id] = new_id;
            }
        }
        return node_id_to_group_id;
    }

    void MProblemsExplainer::create_merged_graph() 
    {
        std::unordered_map<node_id, group_node_id> node_id_to_group_id = create_merged_nodes();
        for (node_id from_node_id = 0; from_node_id < m_initial_conflict_graph.get_node_list().size(); ++from_node_id) 
        {
            group_node_id from_node_group_id = node_id_to_group_id[from_node_id];
            for (const auto& edge : m_initial_conflict_graph.get_edge_list(from_node_id))
            {
                node_id to_node_id = edge.first; 
                group_node_id to_node_group_id = node_id_to_group_id[to_node_id];
                MEdgeInfo edgeInfo = edge.second;
                if (m_union.root(from_node_id) != m_union.root(to_node_id)) 
                {
                    bool found = m_merged_conflict_graph.update_edge_if_present(from_node_group_id, to_node_group_id, edgeInfo);
                    if (!found) 
                    {
                        MGroupEdgeInfo group_edge(edgeInfo);
                        m_merged_conflict_graph.add_edge(from_node_group_id, to_node_group_id, group_edge);
                    }

                }
            }
        }
    }
    
}