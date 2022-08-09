// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_GRAPH_HPP
#define MAMBA_PROBLEMS_GRAPH_HPP

#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <functional>
#include <iostream>

#include "mamba/core/solver_problems.hpp"
#include "mamba/core/property_graph.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/util.hpp"

extern "C"
{
#include "solv/solver.h"
}

namespace mamba
{   
    class MNode
    {
    public:
        MNode(const PackageInfo& package_info, std::optional<SolverRuleinfo> problem_type = std::nullopt);
        MNode(std::string dep, std::optional<SolverRuleinfo> problem_type = std::nullopt);
        MNode();
        
        std::optional<PackageInfo> m_package_info;
        std::optional<std::string> m_dep;
        std::optional<SolverRuleinfo> m_problem_type;
        bool m_is_root;

        bool is_root() const;
        std::string get_name() const;
        bool operator==(const MNode& node) const;

        struct HashFunction
        {
            size_t operator()(const MNode& node) const
            {
                //TODO broken change this!
                if (node.m_package_info.has_value()) {
                    return std::hash<std::string>()(node.m_package_info.value().sha256);
                }
                else if (node.m_dep.has_value()) {
                    return std::hash<std::string>()(node.m_dep.value());
                }
                //assume it is a root
                return 0;
            }
        };
    };

    class MEdgeInfo
    {
    public:
        std::string m_dep;

        MEdgeInfo(std::string dep);
    };

    class MGroupNode
    {
    public:
        bool m_is_root;
        std::optional<std::string> m_dep;
        std::optional<std::string> m_pkg_name;
        std::unordered_set<std::string> m_pkg_versions;
        std::optional<SolverRuleinfo> m_problem_type;

        MGroupNode(const MNode& node);
        MGroupNode();

        void add(const MNode& node);
        std::string get_name() const;
        bool is_root() const;
        //friend std::ostream& operator<<(std::ostream& os, const MGroupNode& edge);
    };

    class MGroupEdgeInfo
    {
    public:
        std::unordered_set<std::string> m_deps;

        MGroupEdgeInfo(const MEdgeInfo& dep);

        void add(MEdgeInfo dep);
        bool operator==(const MGroupEdgeInfo& edge);

        //friend std::ostream& operator<<(std::ostream& os, const MGroupEdgeInfo& edge);
    };

    class MProblemsGraphs
    {
    public: 
        using initial_conflict_graph = MPropertyGraph<MNode, MEdgeInfo>;
        using merged_conflict_graph = MPropertyGraph<MGroupNode, MGroupEdgeInfo>;
        using node_id = initial_conflict_graph::node_id;
        using group_node_id = merged_conflict_graph::node_id;
        using conflicts_node_ids = std::unordered_map<node_id, std::unordered_set<node_id>>;
        using conflicts_group_ids = std::unordered_map<group_node_id, std::unordered_set<group_node_id>>;
        MProblemsGraphs();
        MProblemsGraphs(MPool* pool);
        MProblemsGraphs(MPool* pool, const std::vector<MSolverProblem>& problems);
    
        merged_conflict_graph create_graph(const std::vector<MSolverProblem>& problems);
        
        void add_conflict_edge(MNode from_node, MNode to_node, std::string info);
        node_id get_or_create_node(MNode mnode);
        void add_solvables_to_conflicts(node_id source_node, node_id target_node);
        void create_unions();

        const conflicts_group_ids& get_groups_conflicts();
        merged_conflict_graph create_merged_graph();
        
        Union<node_id> m_union;
        initial_conflict_graph m_initial_conflict_graph;
        merged_conflict_graph m_merged_conflict_graph;

        conflicts_node_ids solvables_to_conflicts;
        conflicts_group_ids group_solvables_to_conflicts;
    
    private:
        MPool* m_pool;

        std::unordered_map<MNode, node_id, MNode::HashFunction> m_node_to_id;
    
        void create_initial_graph(const std::vector<MSolverProblem>& problems);
        void add_problem_to_graph(const MSolverProblem& problem);

        std::unordered_map<node_id, group_node_id> create_merged_nodes();
        std::optional<std::string> get_package_name(node_id id);
    };

    // TODO move to cpp - Linking problems during tests
    inline MNode::MNode(const PackageInfo& package_info, std::optional<SolverRuleinfo> problem_type) 
        : m_package_info(package_info)
        , m_dep(std::nullopt)
        , m_problem_type(problem_type)
        , m_is_root(false) { }
    
    inline MNode::MNode(std::string dep, std::optional<SolverRuleinfo> problem_type) 
        : m_package_info(std::nullopt)
        , m_dep(dep)
        , m_problem_type(problem_type)
        , m_is_root(false) { }
    
    inline MNode::MNode()
        : m_package_info(std::nullopt)
        , m_dep(std::nullopt)
        , m_problem_type(std::nullopt)
        , m_is_root(true) { }

    inline bool MNode::operator== (const MNode& other) const 
    { 
        if (is_root() && other.is_root())
        {
            return true;
        }
        if (m_package_info.has_value() && other.m_package_info.has_value())
        {
            return m_package_info.value() == other.m_package_info.value();
        }
        if (m_dep.has_value() && other.m_dep.has_value()) {
            return m_dep.value() == other.m_dep.value();
        }
        return false;
    }

    inline bool MNode::is_root() const

    {
        return m_is_root;
    }

    inline std::string MNode::get_name() const
    {
        if(is_root())
        {
            return "root";
        }
        else if (m_package_info.has_value())
        {
            return m_package_info.value().name
            ;
        }
        //TODO: throw invalid exception
        return m_dep.has_value() ? m_dep.value() : "invalid";
    }

    inline MEdgeInfo::MEdgeInfo(std::string dep) : m_dep(dep) { }

    /************************
     * group node implementation *
    *************************/

    inline void MGroupNode::add(const MNode& node) 
    { 
        if (node.m_package_info.has_value())
        {
            m_pkg_versions.insert(
                node.m_package_info.value().version);
        }
        m_dep = node.m_dep;
        m_problem_type = node.m_problem_type;
        m_is_root = node.m_is_root;
    }

    inline std::string MGroupNode::get_name() const
    {
        if(is_root())
        {
            return "root";
        }
        else if (m_pkg_name.has_value())
        {
            return m_pkg_name.value();
        }
        return m_dep.has_value() ? m_dep.value() : "invalid";
    }

    inline bool MGroupNode::is_root() const
    {
        return m_is_root;
    }

    inline MGroupNode::MGroupNode(const MNode& node) 
        : m_is_root(node.is_root())
        , m_dep(node.m_dep)
        , m_problem_type(node.m_problem_type)
    {   
        if (node.m_package_info.has_value())
        {
            m_pkg_name = std::optional<std::string>(node.m_package_info.value().name);
            m_pkg_versions.insert(node.m_package_info.value().version);
        }
    }

    inline MGroupNode::MGroupNode() 
        : m_is_root(true)
        , m_dep(std::nullopt)
        , m_pkg_name(std::nullopt)
        , m_pkg_versions({})
        , m_problem_type(std::nullopt) { }

    /************************
     * group edge implementation *
    *************************/

    inline void MGroupEdgeInfo::add(MEdgeInfo edge) 
    {
        m_deps.insert(edge.m_dep);
    }

    inline MGroupEdgeInfo::MGroupEdgeInfo(const MEdgeInfo& edge)
    {
        m_deps.insert(edge.m_dep);
    }

    inline bool MGroupEdgeInfo::operator==(const MGroupEdgeInfo& edge) 
    {
        return m_deps == edge.m_deps;
    }

    /************************
     * problems graph implementation *
    *************************/


    inline MProblemsGraphs::MProblemsGraphs(MPool* pool) : m_pool(pool) {}

    inline MProblemsGraphs::MProblemsGraphs() {}

    inline MProblemsGraphs::MProblemsGraphs(MPool* pool, const std::vector<MSolverProblem>& problems) 
    {
        create_graph(problems);
    }

    inline MProblemsGraphs::merged_conflict_graph MProblemsGraphs::create_graph(const std::vector<MSolverProblem>& problems)
    {
        create_initial_graph(problems);
        create_unions();
        return create_merged_graph();
    }

    inline void MProblemsGraphs::create_initial_graph(const std::vector<MSolverProblem>& problems) 
    {
        for (const auto& problem : problems) 
        {
            add_problem_to_graph(problem);
        }
    }

    inline void MProblemsGraphs::add_problem_to_graph(const MSolverProblem& problem) 
    {
        switch (problem.type) 
        {
            case SOLVER_RULE_PKG_REQUIRES:          // source_id -> dep_id -> target_id
            case SOLVER_RULE_PKG_CONSTRAINS:        // source_id -> dep_id -> <conflict - to - be ignored>
            case SOLVER_RULE_JOB:                   //entry point for our solver
            case SOLVER_RULE_PKG: 
            { 
                std::vector<Id> target_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& target_id : target_ids) 
                {
                    PackageInfo target = PackageInfo(pool_id2solvable(*m_pool, target_id));
                    if (problem.source().has_value())
                    {
                        add_conflict_edge(MNode(problem.source().value()), MNode(target), problem.dep().value());
                    }
                    else
                    {
                        add_conflict_edge(MNode(), MNode(target), problem.dep().value());
                    }
                }
            }
            case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP: 
            case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
            {
                //source_id -> <invalid_dep> -> None
                add_conflict_edge(MNode(problem.source().value()), MNode(problem.dep().value(), problem.type), problem.dep().value());
            }
            case SOLVER_RULE_PKG_CONFLICTS:
            case SOLVER_RULE_PKG_SAME_NAME: 
            {
                // source_id -> <ignore> -> target_id
                node_id source_node = get_or_create_node(MNode(problem.source().value()));
                node_id target_node = get_or_create_node(MNode(problem.target().value()));
                //TODO: optimisation - we might not need this bidirectional
                add_solvables_to_conflicts(source_node, target_node);
            }
            case SOLVER_RULE_BEST:
            case SOLVER_RULE_BLACK:
            case SOLVER_RULE_DISTUPGRADE:
            case SOLVER_RULE_INFARCH:
            case SOLVER_RULE_PKG_NOT_INSTALLABLE:
            case SOLVER_RULE_STRICT_REPO_PRIORITY:
            case SOLVER_RULE_UPDATE:
            {
                if (problem.source().has_value())
                {
                    get_or_create_node(MNode(problem.source().value(), problem.type));
                }
                else
                {
                    LOG_WARNING << "No value provided for source " << problem.type;
                }
                break;
            }
            case SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM:
            {
                //TODO ? dep ?  
                
                LOG_WARNING << "Problem type not implemented: " << problem.type;
                break;
            }
            case SOLVER_RULE_CHOICE:
            case SOLVER_RULE_FEATURE:
            case SOLVER_RULE_JOB_UNSUPPORTED:
            case SOLVER_RULE_LEARNT:
            case SOLVER_RULE_PKG_RECOMMENDS:
            case SOLVER_RULE_RECOMMENDS:
            case SOLVER_RULE_UNKNOWN:
            {
                // TODO: I don't think these are needed?
                // not used in libsolv/problems.c
                LOG_WARNING << "Problem type not implemented: " << problem.type;
                break;
            }
            case SOLVER_RULE_PKG_SELF_CONFLICT:
            case SOLVER_RULE_PKG_OBSOLETES:
            case SOLVER_RULE_PKG_IMPLICIT_OBSOLETES:
            case SOLVER_RULE_PKG_INSTALLED_OBSOLETES:
            case SOLVER_RULE_YUMOBS:
                LOG_WARNING << "Problem type not implemented: " << problem.type;
        }
    }

    inline void MProblemsGraphs::add_conflict_edge(MNode from_node, MNode to_node, std::string info)
    {
        MProblemsGraphs::node_id from_node_id = get_or_create_node(from_node);
        MProblemsGraphs::node_id to_node_id = get_or_create_node(to_node);
        m_initial_conflict_graph.add_edge(from_node_id, to_node_id, info);
    }

    inline MProblemsGraphs::node_id MProblemsGraphs::get_or_create_node(MNode mnode)
    {
        auto it = m_node_to_id.find(mnode);
        if (it == m_node_to_id.end())
        {
            auto node_id = m_initial_conflict_graph.add_node(mnode);
            m_node_to_id.insert(std::make_pair(mnode, node_id));
            return node_id;
        }
        return it->second;
    }

    inline void MProblemsGraphs::add_solvables_to_conflicts(node_id source_node, node_id target_node)
    {
        solvables_to_conflicts[source_node].insert(target_node);
        solvables_to_conflicts[target_node].insert(source_node);
    }

    inline void MProblemsGraphs::create_unions() 
    {   
        size_t nodes_size = m_initial_conflict_graph.get_node_list().size();
        std::vector<std::unordered_set<node_id>> node_to_neigh(nodes_size);
        std::unordered_map<size_t, std::vector<node_id>> hashes_to_nodes;
        for (node_id i = 0; i < nodes_size; ++i) 
        {
            std::vector<std::pair<node_id, MEdgeInfo>> edge_list = m_initial_conflict_graph.get_edge_list(i);
            std::unordered_set<node_id> neighs;
            std::transform(
                edge_list.begin(), 
                edge_list.end(), 
                std::inserter(neighs, neighs.begin()),
                [](auto &kv){ return kv.first;});
            node_to_neigh[i] = neighs;
            hashes_to_nodes[hash<node_id>(neighs)].push_back(i);
            m_union.add(i);
        }

        // need to also merge the leaves - the childrens will be the conflicts from same_name 
        for (const auto& solvable_to_conflict : solvables_to_conflicts) 
        {
            hashes_to_nodes[hash<node_id>(solvable_to_conflict.second)].push_back(solvable_to_conflict.first);
            node_to_neigh[solvable_to_conflict.first].insert(solvable_to_conflict.second.begin(), solvable_to_conflict.second.end());
            m_union.add(solvable_to_conflict.first);
        }

        // we go through the list of conflicts and make sure that we can merge the information
        for (const auto& hash_to_nodes : hashes_to_nodes)
        {
            for (size_t i = 0; i < hash_to_nodes.second.size(); ++i)
            {
                node_id id_i = hash_to_nodes.second[i];
                std::optional<std::string> maybe_package_name_i = get_package_name(id_i);
                if (!maybe_package_name_i.has_value())
                {
                    continue;
                }
                std::cerr << "trying " << id_i << " " << maybe_package_name_i.value() << std::endl;
                for (size_t j = i+1; j < hash_to_nodes.second.size(); ++j) 
                {
                    node_id id_j = hash_to_nodes.second[j];
                    std::optional<std::string> maybe_package_name_j = get_package_name(id_j);
                    if (!maybe_package_name_j.has_value()) 
                    {
                        continue;
                    }
                    
                    /*std::cerr << "\t with " << id_j << " " << maybe_package_name_j.value() << std::endl;
                    std::cerr << "\t]\t rev_ edge: "
                         << join_str(m_initial_conflict_graph.get_rev_edge_list(id_i)) << " and " <<  
                            join_str
                            (m_initial_conflict_graph.get_rev_edge_list(id_j)) << std::endl;
                    */
                    if (maybe_package_name_i == maybe_package_name_j
                        &&  node_to_neigh[id_i] == node_to_neigh[id_j]
                        && m_initial_conflict_graph.get_rev_edge_list(id_i) 
                            == m_initial_conflict_graph.get_rev_edge_list(id_j))
                    {
                        std::cerr << "connected " << id_i << "(" 
                            << maybe_package_name_i.value() << ") " 
                            << id_j << "(" << maybe_package_name_j.value()
                             << ")"<< std::endl;
                        m_union.connect(id_i, id_j);
                    }
                }
            }
        }
    }

    inline std::optional<std::string> MProblemsGraphs::get_package_name(MProblemsGraphs::node_id id) {
        auto maybe_package_info = m_initial_conflict_graph.get_node(id).m_package_info;
        if (maybe_package_info.has_value()) 
        {
            return maybe_package_info.value().name;
        }
        return std::nullopt;
    }

    inline MProblemsGraphs::merged_conflict_graph MProblemsGraphs::create_merged_graph() 
    {
        std::unordered_map<node_id, group_node_id> root_to_group_id = create_merged_nodes();
        for (node_id from_node_id = 0; from_node_id < m_initial_conflict_graph.get_node_list().size(); ++from_node_id) 
        {
            node_id from_node_root = m_union.root(from_node_id);
            group_node_id from_node_group_id = root_to_group_id[from_node_root];
            for (const auto& edge : m_initial_conflict_graph.get_edge_list(from_node_id))
            {
                node_id to_node_id = edge.first; 
                node_id to_node_root = m_union.root(to_node_id);
                group_node_id to_node_group_id = root_to_group_id[to_node_root];
                MEdgeInfo edgeInfo = edge.second;
                if (from_node_root != to_node_root) 
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
        return m_merged_conflict_graph;
    }

    inline auto MProblemsGraphs::get_groups_conflicts() -> const conflicts_group_ids&
    {
        return group_solvables_to_conflicts;
    }

    inline std::unordered_map<MProblemsGraphs::node_id, MProblemsGraphs::group_node_id> MProblemsGraphs::create_merged_nodes() {
        std::unordered_map<node_id, group_node_id> root_to_group_id;
        std::unordered_map<node_id, node_id> parents = m_union.parent;
        std::vector<node_id> keys(parents.size());
        auto key_selector = [](auto pair){return pair.first;};
        std::transform(parents.begin(), parents.end(), keys.begin(), key_selector);
        // go through each one of them and set the information to its info & its kids 
        for (const auto& id : keys) {
            MNode node = m_initial_conflict_graph.get_node(id);
            node_id root = m_union.root(id);
            auto it = root_to_group_id.find(root);
            if (it != root_to_group_id.end()) 
            {
                m_merged_conflict_graph.update_node(it->second, node);
            } 
            else 
            {
                MGroupNode group_node(node);
                root_to_group_id[id] = m_merged_conflict_graph.add_node(group_node);
                /*std::cerr << "Created new group node " << std::to_string(new_id) 
                    << " " <<  node.m_package_info.value().name
                    << " " << node.m_package_info.value().version

                     << std::endl;*/
            }
        }

        //updating the conflicts map
        for (const auto& solvable_to_conflict : solvables_to_conflicts) 
        {
            auto id = root_to_group_id[m_union.root(solvable_to_conflict.first)];
            for (const auto& conflict : solvable_to_conflict.second)
            {
                auto conflict_id = root_to_group_id[m_union.root(conflict)];
                group_solvables_to_conflicts[id].insert(conflict_id);
            }
        }
        return root_to_group_id;
    }

    inline std::ostream& operator<<(std::ostream& strm, const MGroupNode& node) {
        if (node.m_pkg_name.has_value())
        {
            return strm << "package " << node.m_pkg_name.value() 
                << " versions [" 
                << join(node.m_pkg_versions) + "]";
        } 
        else
        {
            return strm << "No packages matching " 
                << node.m_dep.value() << " could be found in the provided channels";
        }
    }

    inline std::ostream& operator<<(std::ostream& strm, const MGroupEdgeInfo& edge) {
        return strm << join(edge.m_deps);
        
    }
}

#endif //MAMBA_PROBLEMS_GRAPH_HPP
