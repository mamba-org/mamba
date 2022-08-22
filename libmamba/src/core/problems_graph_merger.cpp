// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/problems_graph_merger.hpp"

namespace mamba
{

    MProblemsGraphMerger::MProblemsGraphMerger(const initial_graph& graph)
        : m_initial_graph(graph)
        , m_merged_graph()
    {
    }

    void MProblemsGraphMerger::create_unions()
    {
        size_t nodes_size = m_initial_graph.get_node_list().size();
        std::vector<std::set<node_id>> node_to_neigh(nodes_size);
        std::unordered_map<size_t, std::vector<node_id>> hashes_to_nodes;
        for (node_id i = 0; i < nodes_size; ++i)
        {
            std::vector<std::pair<node_id, MEdgeInfo>> edge_list = m_initial_graph.get_edge_list(i);
            std::set<node_id> neighs;
            std::transform(edge_list.begin(),
                           edge_list.end(),
                           std::inserter(neighs, neighs.begin()),
                           [](auto& kv) { return kv.first; });
            node_to_neigh[i] = neighs;
            hashes_to_nodes[hash(neighs)].push_back(i);
            m_union.add(i);
        }

        // need to also merge the leaves - the childrens will be the conflicts from same_name
        for (const auto& solvable_to_conflict : m_initial_graph.get_conflicts())
        {
            hashes_to_nodes[hash(solvable_to_conflict.second)].push_back(
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
                std::optional<std::string> maybe_package_name_i
                    = m_initial_graph.get_package_name(id_i);
                if (!maybe_package_name_i)
                {
                    continue;
                }
                for (size_t j = i + 1; j < hash_to_nodes.second.size(); ++j)
                {
                    node_id id_j = hash_to_nodes.second[j];
                    std::optional<std::string> maybe_package_name_j
                        = m_initial_graph.get_package_name(id_j);
                    if (!maybe_package_name_j)
                    {
                        continue;
                    }

                    if (maybe_package_name_i == maybe_package_name_j
                        && node_to_neigh[id_i] == node_to_neigh[id_j]
                        && m_initial_graph.get_rev_edge_list(id_i)
                               == m_initial_graph.get_rev_edge_list(id_j))
                    {
                        m_union.connect(id_i, id_j);
                    }
                }
            }
        }
    }

    auto MProblemsGraphMerger::create_merged_nodes() -> node_id_to_group_id
    {
        node_id_to_group_id root_to_group_id;
        size_t nodes_size = m_initial_graph.get_node_list().size();
        for (node_id id = 0; id < nodes_size; ++id)
        {
            MNode node = m_initial_graph.get_node(id);
            node_id root = m_union.root(id);
            auto it = root_to_group_id.find(root);
            if (it != root_to_group_id.end())
            {
                m_merged_graph.update_node(it->second, node);
            }
            else
            {
                MGroupNode group_node(node);
                root_to_group_id[root] = m_merged_graph.add_node(group_node);
            }
        }

        // updating the conflicts map
        for (const auto& solvable_to_conflict : m_initial_graph.get_conflicts())
        {
            auto id = root_to_group_id[m_union.root(solvable_to_conflict.first)];
            for (const auto& conflict : solvable_to_conflict.second)
            {
                auto conflict_id = root_to_group_id[m_union.root(conflict)];
                m_merged_graph.add_conflicts(id, conflict_id);
            }
        }
        return root_to_group_id;
    }

    auto MProblemsGraphMerger::create_merged_graph() -> const merged_graph&
    {
        create_unions();
        auto unions = m_union.get_unions();
        for (const auto& root_to_groups : unions)
        {
            std::cout << "For parent " << m_initial_graph.get_node(root_to_groups.first) << "("
                      << root_to_groups.first << ")" << std::endl;
            for (const auto& node : root_to_groups.second)
            {
                std::cout << "\t" << m_initial_graph.get_node(node) << "(" << node << ")"
                          << std::endl;
            }
        }
        std::unordered_map<node_id, group_node_id> root_to_group_id = create_merged_nodes();
        for (node_id from_node_id = 0; from_node_id < m_initial_graph.get_node_list().size();
             ++from_node_id)
        {
            node_id from_node_root = m_union.root(from_node_id);
            group_node_id from_node_group_id = root_to_group_id[from_node_root];
            for (const auto& edge : m_initial_graph.get_edge_list(from_node_id))
            {
                node_id to_node_id = edge.first;
                MEdgeInfo edgeInfo = edge.second;
                node_id to_node_root = m_union.root(to_node_id);
                group_node_id to_node_group_id = root_to_group_id[to_node_root];
                if (from_node_root != to_node_root)
                {
                    bool found = m_merged_graph.update_edge_if_present(
                        from_node_group_id, to_node_group_id, edgeInfo);
                    if (!found)
                    {
                        MGroupEdgeInfo group_edge(edgeInfo);
                        m_merged_graph.add_edge(from_node_group_id, to_node_group_id, group_edge);
                    }
                }
            }
        }
        return m_merged_graph;
    }
}
