// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/problems_graph.hpp"

namespace mamba
{
    MNode::MNode(const PackageInfo& package_info, std::optional<SolverRuleinfo> problem_type)
        : m_package_info(package_info)
        , m_dep(std::nullopt)
        , m_problem_type(problem_type)
        , m_is_root(false)
    {
    }

    MNode::MNode(std::string dep, std::optional<SolverRuleinfo> problem_type)
        : m_package_info(std::nullopt)
        , m_dep(dep)
        , m_problem_type(problem_type)
        , m_is_root(false)
    {
    }

    MNode::MNode()
        : m_package_info(std::nullopt)
        , m_dep(std::nullopt)
        , m_problem_type(std::nullopt)
        , m_is_root(true)
    {
    }

    bool MNode::operator==(const MNode& other) const
    {
        return m_package_info == other.m_package_info && m_dep == other.m_dep
               && m_is_root == other.m_is_root;
    }

    bool MNode::is_root() const
    {
        return m_is_root;
    }

    bool MNode::is_conflict() const
    {
        if (is_root())
        {
            return false;
        }

        // TODO maybe change it here
        return !m_problem_type;
    }

    std::string MNode::get_name() const
    {
        if (is_root())
        {
            return "root";
        }
        else if (m_package_info)
        {
            return m_package_info.value().name;
        }
        // TODO: throw invalid exception
        return m_dep ? m_dep.value() : "invalid";
    }

    MEdgeInfo::MEdgeInfo(std::string dep)
        : m_dep(dep)
    {
    }

    /************************
     * group node implementation *
     *************************/

    void MGroupNode::add(const MNode& node)
    {
        if (node.m_package_info)
        {
            m_pkg_versions.insert(node.m_package_info.value().version);
        }
        m_dep = node.m_dep;
        m_problem_type = node.m_problem_type;
        m_is_root = node.m_is_root;
    }

    std::string MGroupNode::get_name() const
    {
        if (is_root())
        {
            return "root";
        }
        else if (m_pkg_name)
        {
            return m_pkg_name.value();
        }
        return m_dep ? m_dep.value() : "invalid";
    }

    bool MGroupNode::is_root() const
    {
        return m_is_root;
    }

    bool MGroupNode::is_conflict() const
    {
        if (is_root())
        {
            return false;
        }

        // TODO maybe change it here
        return !m_problem_type;
    }

    MGroupNode::MGroupNode(const MNode& node)
        : m_is_root(node.is_root())
        , m_dep(node.m_dep)
        , m_problem_type(node.m_problem_type)
    {
        if (node.m_package_info)
        {
            m_pkg_name = std::optional<std::string>(node.m_package_info.value().name);
            m_pkg_versions.insert(node.m_package_info.value().version);
        }
        else
        {
            m_pkg_name = std::nullopt;
        }
    }

    MGroupNode::MGroupNode()
        : m_is_root(true)
        , m_dep(std::nullopt)
        , m_pkg_name(std::nullopt)
        , m_pkg_versions({})
        , m_problem_type(std::nullopt)
    {
    }

    /************************
     * group edge implementation *
     *************************/

    void MGroupEdgeInfo::add(MEdgeInfo edge)
    {
        m_deps.insert(edge.m_dep);
    }

    MGroupEdgeInfo::MGroupEdgeInfo(const MEdgeInfo& edge)
    {
        m_deps.insert(edge.m_dep);
    }

    bool MGroupEdgeInfo::operator==(const MGroupEdgeInfo& edge)
    {
        return m_deps == edge.m_deps;
    }

    /************************
     * problems graph implementation *
     *************************/


    MProblemsGraphs::MProblemsGraphs(MPool* pool)
        : m_pool(pool)
    {
    }

    MProblemsGraphs::MProblemsGraphs()
    {
    }

    MProblemsGraphs::MProblemsGraphs(MPool* pool, const std::vector<MSolverProblem>& problems)
    {
        create_graph(problems);
    }

    MProblemsGraphs::merged_conflict_graph MProblemsGraphs::create_graph(
        const std::vector<MSolverProblem>& problems)
    {
        create_initial_graph(problems);
        create_unions();
        return create_merged_graph();
    }

    void MProblemsGraphs::create_initial_graph(const std::vector<MSolverProblem>& problems)
    {
        for (const auto& problem : problems)
        {
            std::cerr << problem.to_string() << std::endl;
        }
        std::cerr << "----" << std::endl;
        for (const auto& problem : problems)
        {
            add_problem_to_graph(problem);
        }
    }

    void MProblemsGraphs::add_problem_to_graph(const MSolverProblem& problem)
    {
        std::cerr << problem.to_string() << std::endl;
        std::cerr << problem.type << " " << get_value_or(problem.source()) << " "
                  << get_value_or(problem.target()) << " " << problem.dep().value_or("None")
                  << std::endl;
        switch (problem.type)
        {
            case SOLVER_RULE_PKG_CONSTRAINS:  // source_id -> dep_id -> <conflict>
            {
                if (!has_values(problem, { problem.source(), problem.target() })
                    || !has_values(problem, { problem.dep() }))
                {
                    break;
                }
                auto source = MNode(problem.source().value());
                std::vector<Id> constrained_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& constraint_id : constrained_ids)
                {
                    PackageInfo constraint_pkg
                        = PackageInfo(pool_id2solvable(*m_pool, constraint_id));
                    // std::cerr << "target = " << constraint_pkg.str() << " " << problem.dep() <<
                    // std::endl;
                    add_conflict_edge(source, MNode(constraint_pkg), problem.dep().value());
                }
                node_id source_id = get_or_create_node(source);
                node_id target_id = get_or_create_node(MNode(problem.target().value()));
                add_solvables_to_conflicts(source_id, target_id);
                break;
            }
            case SOLVER_RULE_PKG_REQUIRES:  // source_id -> dep_id -> target_id
            case SOLVER_RULE_JOB:           // entry point for our solver
            case SOLVER_RULE_PKG:
            {
                if (!has_values(problem, { problem.dep() }))
                {
                    break;
                }
                std::vector<Id> target_ids = m_pool->select_solvables(problem.dep_id);
                for (const auto& target_id : target_ids)
                {
                    PackageInfo target = PackageInfo(pool_id2solvable(*m_pool, target_id));
                    // std::cerr << "target = " << target.str() << " " << problem.dep() <<
                    // std::endl;
                    if (problem.source())
                    {
                        add_conflict_edge(
                            MNode(problem.source().value()), MNode(target), problem.dep().value());
                    }
                    else
                    {
                        add_conflict_edge(MNode(), MNode(target), problem.dep().value());
                    }
                }
                break;
            }
            case SOLVER_RULE_JOB_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_PKG_NOTHING_PROVIDES_DEP:
            case SOLVER_RULE_JOB_UNKNOWN_PACKAGE:
            {
                if (!has_values(problem, { problem.source() })
                    || !has_values(problem, { problem.dep() }))
                {
                    break;
                }
                // source_id -> <invalid_dep> -> None
                add_conflict_edge(MNode(problem.source().value()),
                                  MNode(problem.dep().value(), problem.type),
                                  problem.dep().value());
                break;
            }
            case SOLVER_RULE_PKG_CONFLICTS:
            case SOLVER_RULE_PKG_SAME_NAME:
            {
                if (!has_values(problem, { problem.source(), problem.target() }))
                {
                    break;
                }
                // source_id -> <ignore> -> target_id
                node_id source_node = get_or_create_node(MNode(problem.source().value()));
                node_id target_node = get_or_create_node(MNode(problem.target().value()));
                // TODO: optimisation - we might not need this bidirectional
                add_solvables_to_conflicts(source_node, target_node);
                break;
            }
            case SOLVER_RULE_BEST:
            case SOLVER_RULE_BLACK:
            case SOLVER_RULE_DISTUPGRADE:
            case SOLVER_RULE_INFARCH:
            case SOLVER_RULE_PKG_NOT_INSTALLABLE:
            case SOLVER_RULE_STRICT_REPO_PRIORITY:
            case SOLVER_RULE_UPDATE:
            {
                if (!has_values(problem, { problem.source() }))
                {
                    break;
                }
                get_or_create_node(MNode(problem.source().value(), problem.type));
                break;
            }
            case SOLVER_RULE_JOB_PROVIDED_BY_SYSTEM:
            {
                // TODO ? dep ?

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

    void MProblemsGraphs::add_conflict_edge(MNode from_node, MNode to_node, std::string info)
    {
        MProblemsGraphs::node_id from_node_id = get_or_create_node(from_node);
        MProblemsGraphs::node_id to_node_id = get_or_create_node(to_node);
        m_initial_conflict_graph.add_edge(from_node_id, to_node_id, info);
    }

    MProblemsGraphs::node_id MProblemsGraphs::get_or_create_node(MNode mnode)
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

    void MProblemsGraphs::add_solvables_to_conflicts(node_id source_node, node_id target_node)
    {
        solvables_to_conflicts[source_node].insert(target_node);
        solvables_to_conflicts[target_node].insert(source_node);
    }

    void MProblemsGraphs::create_unions()
    {
        size_t nodes_size = m_initial_conflict_graph.get_node_list().size();
        std::vector<std::set<node_id>> node_to_neigh(nodes_size);
        std::unordered_map<size_t, std::vector<node_id>> hashes_to_nodes;
        for (node_id i = 0; i < nodes_size; ++i)
        {
            std::vector<std::pair<node_id, MEdgeInfo>> edge_list
                = m_initial_conflict_graph.get_edge_list(i);
            std::set<node_id> neighs;
            std::transform(edge_list.begin(),
                           edge_list.end(),
                           std::inserter(neighs, neighs.begin()),
                           [](auto& kv) { return kv.first; });
            node_to_neigh[i] = neighs;
            hashes_to_nodes[hash<node_id>(neighs)].push_back(i);
            m_union.add(i);
        }

        // need to also merge the leaves - the childrens will be the conflicts from same_name
        for (const auto& solvable_to_conflict : solvables_to_conflicts)
        {
            hashes_to_nodes[hash<node_id>(solvable_to_conflict.second)].push_back(
                solvable_to_conflict.first);
            node_to_neigh[solvable_to_conflict.first].insert(solvable_to_conflict.second.begin(),
                                                             solvable_to_conflict.second.end());
            m_union.add(solvable_to_conflict.first);
        }

        // we go through the list of conflicts and make sure that we can merge the information
        for (const auto& hash_to_nodes : hashes_to_nodes)
        {
            for (size_t i = 0; i < hash_to_nodes.second.size(); ++i)
            {
                node_id id_i = hash_to_nodes.second[i];
                std::optional<std::string> maybe_package_name_i = get_package_name(id_i);
                auto node_i = m_initial_conflict_graph.get_node(id_i);
                if (!maybe_package_name_i)
                {
                    continue;
                }
                std::cerr << "trying " << id_i << " " << node_i.m_package_info.value().str()
                          << std::endl;
                for (size_t j = i + 1; j < hash_to_nodes.second.size(); ++j)
                {
                    node_id id_j = hash_to_nodes.second[j];
                    std::optional<std::string> maybe_package_name_j = get_package_name(id_j);
                    if (!maybe_package_name_j)
                    {
                        continue;
                    }

                    /*std::cerr << "\t with " << id_j << " " << maybe_package_name_j.value() <<
                    std::endl; std::cerr << "\t]\t rev_ edge: "
                         << join(m_initial_conflict_graph.get_rev_edge_list(id_i)) << " and " <<
                            join
                            (m_initial_conflict_graph.get_rev_edge_list(id_j)) << std::endl;
                    */
                    if (maybe_package_name_i == maybe_package_name_j
                        && node_to_neigh[id_i] == node_to_neigh[id_j]
                        && m_initial_conflict_graph.get_rev_edge_list(id_i)
                               == m_initial_conflict_graph.get_rev_edge_list(id_j))
                    {
                        auto node_j = m_initial_conflict_graph.get_node(id_j);
                        std::cerr << "connected " << id_i << "("
                                  << node_i.m_package_info.value().str() << ") " << id_j << "("
                                  << node_j.m_package_info.value().str() << ")" << std::endl;
                        m_union.connect(id_i, id_j);
                    }
                }
            }
        }
    }

    std::optional<std::string> MProblemsGraphs::get_package_name(MProblemsGraphs::node_id id)
    {
        auto node = m_initial_conflict_graph.get_node(id);
        if (node.m_package_info)
        {
            return node.m_package_info.value().name;
        }
        return std::nullopt;
    }

    MProblemsGraphs::merged_conflict_graph MProblemsGraphs::create_merged_graph()
    {
        std::unordered_map<node_id, group_node_id> root_to_group_id = create_merged_nodes();
        for (node_id from_node_id = 0;
             from_node_id < m_initial_conflict_graph.get_node_list().size();
             ++from_node_id)
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
                    bool found = m_merged_conflict_graph.update_edge_if_present(
                        from_node_group_id, to_node_group_id, edgeInfo);
                    if (!found)
                    {
                        MGroupEdgeInfo group_edge(edgeInfo);
                        m_merged_conflict_graph.add_edge(
                            from_node_group_id, to_node_group_id, group_edge);
                    }
                }
            }
        }
        return m_merged_conflict_graph;
    }

    auto MProblemsGraphs::get_groups_conflicts() -> const conflicts_group_ids&
    {
        return group_solvables_to_conflicts;
    }

    std::unordered_map<MProblemsGraphs::node_id, MProblemsGraphs::group_node_id>
    MProblemsGraphs::create_merged_nodes()
    {
        std::unordered_map<node_id, group_node_id> root_to_group_id;
        std::unordered_map<node_id, node_id> parents = m_union.parent;
        std::vector<node_id> keys(parents.size());
        auto key_selector = [](auto pair) { return pair.first; };
        std::transform(parents.begin(), parents.end(), keys.begin(), key_selector);
        // go through each one of them and set the information to its info & its kids
        for (const auto& id : keys)
        {
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
            }
        }

        // updating the conflicts map
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
}
