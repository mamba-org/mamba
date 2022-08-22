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

#include "mamba/core/property_graph.hpp"
#include "mamba/core/package_info.hpp"
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
        MNode(const PackageInfo& package_info,
              std::optional<SolverRuleinfo> problem_type = std::nullopt);
        MNode(std::string dep, std::optional<SolverRuleinfo> problem_type = std::nullopt);
        MNode();

        std::optional<PackageInfo> m_package_info;
        std::optional<std::string> m_dep;
        std::optional<SolverRuleinfo> m_problem_type;
        bool m_is_root;

        std::string get_name() const;
        bool is_root() const;
        bool is_conflict() const;

        bool operator==(const MNode& node) const;

        struct HashFunction
        {
            size_t operator()(const MNode& node) const
            {
                if (node.m_package_info)
                {
                    return PackageInfoHash()(node.m_package_info.value());
                }
                else if (node.m_dep)
                {
                    return std::hash<std::string>()(node.m_dep.value());
                }
                // root
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
        bool is_conflict() const;

        bool operator==(const MGroupNode& node) const;

        struct HashFunction
        {
            size_t operator()(const MGroupNode& node) const
            {
                if (node.m_pkg_name)
                {
                    std::vector<std::string> name_versions(node.m_pkg_versions.begin(),
                                                           node.m_pkg_versions.end());
                    name_versions.push_back(node.m_pkg_name.value());
                    return hash(name_versions);
                }
                else if (node.m_dep)
                {
                    return std::hash<std::string>()(node.m_dep.value());
                }
                // root
                return 0;
            }
        };
    };

    class MGroupEdgeInfo
    {
    public:
        std::set<std::string> m_deps;

        MGroupEdgeInfo(const MEdgeInfo& dep);

        void add(MEdgeInfo dep);
        bool operator==(const MGroupEdgeInfo& edge);
    };

    template <class T, class U>
    class MProblemsGraph
    {
    public:
        using graph = MPropertyGraph<T, U>;
        using node = typename MPropertyGraph<T, U>::node;
        using node_id = typename MPropertyGraph<T, U>::node_id;
        using node_list = typename MPropertyGraph<T, U>::node_list;
        using edge_list = typename MPropertyGraph<T, U>::edge_list;
        using neigh_set = typename MPropertyGraph<T, U>::neigh_set;
        using node_id_to_conflicts = std::unordered_map<node_id, std::set<node_id>>;
        using node_to_node_id = std::unordered_map<T, node_id, typename T::HashFunction>;

        void add_edge(T from, T to, U info);
        void add_edge(node_id from, node_id to, U info);

        node_id add_node(T node);
        node_id add_node(node_id node);

        void add_conflicts(T conflict_i, T conflict_j);
        void add_conflicts(node_id conflict_i, node_id conflict_j);

        template <class V>
        void update_node(node_id id, V info);

        template <class V>
        bool update_edge_if_present(node_id from, node_id to, V info);

        node_id get_or_create_node(T node);
        std::string get_package_name(node_id id) const;
        const node_id_to_conflicts& get_conflicts() const;
        const node& get_node(node_id) const;
        const node_list& get_node_list() const;
        const edge_list& get_edge_list(node_id id) const;
        const neigh_set& get_rev_edge_list(node_id id) const;

        const graph& get_graph() const;

    private:
        graph m_graph;
        node_id_to_conflicts m_nodes_to_conflicts;
        node_to_node_id m_node_to_id;
    };

    inline std::ostream& operator<<(std::ostream& strm, const MGroupNode& node)
    {
        if (node.m_pkg_name)
        {
            return strm << "package " << node.m_pkg_name.value() << " versions ["
                        << join(node.m_pkg_versions) + "]";
        }
        else if (node.m_dep)
        {
            return strm << "No packages matching " << node.m_dep.value()
                        << " could be found in the provided channels";
        }
        else if (node.is_root())
        {
            return strm << "root";
        }
        return strm << "invalid";
    }

    inline std::ostream& operator<<(std::ostream& strm, const MGroupEdgeInfo& edge)
    {
        return strm << join(edge.m_deps);
    }

    inline MNode::MNode(const PackageInfo& package_info, std::optional<SolverRuleinfo> problem_type)
        : m_package_info(package_info)
        , m_dep(std::nullopt)
        , m_problem_type(problem_type)
        , m_is_root(false)
    {
    }

    inline MNode::MNode(std::string dep, std::optional<SolverRuleinfo> problem_type)
        : m_package_info(std::nullopt)
        , m_dep(dep)
        , m_problem_type(problem_type)
        , m_is_root(false)
    {
    }

    inline MNode::MNode()
        : m_package_info(std::nullopt)
        , m_dep(std::nullopt)
        , m_problem_type(std::nullopt)
        , m_is_root(true)
    {
    }

    inline bool MNode::operator==(const MNode& other) const
    {
        return m_package_info == other.m_package_info && m_dep == other.m_dep
               && m_is_root == other.m_is_root;
    }

    inline bool MNode::is_root() const
    {
        return m_is_root;
    }

    inline bool MNode::is_conflict() const
    {
        if (is_root())
        {
            return false;
        }

        // TODO maybe change it here
        return !m_problem_type;
    }

    inline std::string MNode::get_name() const
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

    inline MEdgeInfo::MEdgeInfo(std::string dep)
        : m_dep(dep)
    {
    }

    /************************
     * group node implementation *
     *************************/

    inline void MGroupNode::add(const MNode& node)
    {
        if (node.m_package_info)
        {
            m_pkg_versions.insert(node.m_package_info.value().version + "-"
                                  + node.m_package_info.value().build_string);
        }
        m_dep = node.m_dep;
        m_problem_type = node.m_problem_type;
        m_is_root = node.m_is_root;
    }

    inline std::string MGroupNode::get_name() const
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

    inline bool MGroupNode::is_root() const
    {
        return m_is_root;
    }

    inline bool MGroupNode::is_conflict() const
    {
        if (is_root())
        {
            return false;
        }

        // TODO maybe change it here
        return !m_problem_type;
    }

    inline MGroupNode::MGroupNode(const MNode& node)
        : m_is_root(node.is_root())
        , m_dep(node.m_dep)
        , m_problem_type(node.m_problem_type)
    {
        if (node.m_package_info)
        {
            m_pkg_name = std::optional<std::string>(node.m_package_info.value().name);
            m_pkg_versions.insert(node.m_package_info.value().version + "-"
                                  + node.m_package_info.value().build_string);
        }
        else
        {
            m_pkg_name = std::nullopt;
        }
    }

    inline MGroupNode::MGroupNode()
        : m_is_root(true)
        , m_dep(std::nullopt)
        , m_pkg_name(std::nullopt)
        , m_pkg_versions({})
        , m_problem_type(std::nullopt)
    {
    }

    inline bool MGroupNode::operator==(const MGroupNode& node) const
    {
        return m_dep == node.m_dep && m_pkg_name == node.m_pkg_name
               && m_problem_type == node.m_problem_type && m_pkg_versions == node.m_pkg_versions;
    }

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

    /*********************************
     * problems graph implementation *
     *********************************/

    template <class T, class U>
    inline void MProblemsGraph<T, U>::add_edge(T from, T to, U info)
    {
        node_id from_node_id = get_or_create_node(from);
        node_id to_node_id = get_or_create_node(to);
        add_edge(from_node_id, to_node_id, info);
    }

    template <class T, class U>
    inline void MProblemsGraph<T, U>::add_edge(node_id from, node_id to, U info)
    {
        m_graph.add_edge(from, to, info);
    }

    template <class T, class U>
    inline auto MProblemsGraph<T, U>::add_node(T node) -> node_id
    {
        return get_or_create_node(node);
    }

    template <class T, class U>
    inline void MProblemsGraph<T, U>::add_conflicts(T conflict_i, T conflict_j)
    {
        node_id id_i = get_or_create_node(conflict_i);
        node_id id_j = get_or_create_node(conflict_j);
        add_conflicts(id_i, id_j);
    }

    template <class T, class U>
    inline void MProblemsGraph<T, U>::add_conflicts(node_id conflict_i, node_id conflict_j)
    {
        m_nodes_to_conflicts[conflict_i].insert(conflict_j);
        m_nodes_to_conflicts[conflict_j].insert(conflict_i);
    }

    template <class T, class U>
    template <class V>
    inline void MProblemsGraph<T, U>::update_node(node_id id, V info)
    {
        m_graph.update_node(id, info);
    }

    template <class T, class U>
    template <class V>
    inline bool MProblemsGraph<T, U>::update_edge_if_present(node_id from, node_id to, V info)
    {
        return m_graph.update_edge_if_present(from, to, info);
    }

    template <class T, class U>
    inline auto MProblemsGraph<T, U>::get_or_create_node(T mnode) -> node_id
    {
        auto it = m_node_to_id.find(mnode);
        if (it == m_node_to_id.end())
        {
            auto node_id = m_graph.add_node(mnode);
            m_node_to_id.insert(std::make_pair(mnode, node_id));
            return node_id;
        }
        return it->second;
    }

    template <class T, class U>
    inline auto MProblemsGraph<T, U>::get_node(node_id id) const -> const node&
    {
        return m_graph.get_node(id);
    }

    template <class T, class U>
    inline auto MProblemsGraph<T, U>::get_node_list() const -> const node_list&
    {
        return m_graph.get_node_list();
    }

    template <class T, class U>
    inline auto MProblemsGraph<T, U>::get_edge_list(node_id id) const -> const edge_list&
    {
        return m_graph.get_edge_list(id);
    }

    template <class T, class U>
    inline auto MProblemsGraph<T, U>::get_rev_edge_list(node_id id) const -> const neigh_set&
    {
        return m_graph.get_rev_edge_list(id);
    }

    template <class T, class U>
    inline auto MProblemsGraph<T, U>::get_package_name(node_id id) const -> std::string
    {
        auto node = m_graph.get_node(id);
        return node.get_name();
    }

    template <class T, class U>
    inline auto MProblemsGraph<T, U>::get_conflicts() const -> const node_id_to_conflicts&
    {
        return m_nodes_to_conflicts;
    }

    template <class T, class U>
    auto MProblemsGraph<T, U>::get_graph() const -> const graph&
    {
        return m_graph;
    }
}

#endif  // MAMBA_PROBLEMS_GRAPH_HPP
