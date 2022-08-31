// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.
#include "mamba/core/problems_graph_util.hpp"

namespace mamba
{
    DependencyInfo::DependencyInfo(const std::string& dep)
    {
        std::regex regexp("\\s*(\\w[\\w-]*)\\s*(.*)\\s*");
        std::smatch matches;
        bool matched = std::regex_search(dep, matches, regexp);
        THROWIF(!matched || matches.size() != 2, "unable to parse dep");

        name = matches[0].str();
        range = matches[1].str();
    }

    std::string const& DependencyInfo::get_name() const
    {
        return name;
    }

    std::string const& DependencyInfo::get_range() const
    {
        return range;
    }

    std::string DependencyInfo::str() const
    {
        return name + " " + range;
    }

    MNode::MNode(node_info const& info, std::optional<ProblemType> problem_type)
        : m_info(info)
        , m_problem_type(problem_type)
    {
    }

    std::string MNode::get_name() const
    {
        std::string root_name("root");
        return std::visit(
            overload{
                [](NodeInfo::ResolvedPackage const& node) { return node.m_package_info.name; },
                [](NodeInfo::ProblematicPackage const& node) { return node.m_dep; },
                [&root_name](NodeInfo::Root const& node) { return root_name; },
            },
            m_info);
    }

    bool MNode::is_root() const
    {
        return std::visit(
            overload{
                [](NodeInfo::ResolvedPackage const& node) { return false; },
                [](NodeInfo::ProblematicPackage const& node) { return false; },
                [](NodeInfo::Root const& node) { return true; },
            },
            m_info);
    }

    void MNode::maybe_update_metadata(const MNode& other)
    {
        if (m_problem_type || !other.get_problem_type())
        {
            return;
        }
        m_problem_type = other.get_problem_type();
    }

    auto MNode::get_problem_type() const -> std::optional<ProblemType>
    {
        return m_problem_type;
    }

    MEdge::MEdge(edge_info const& info)
        : m_info(info)
    {
    }

    std::string MEdge::get_info() const
    {
        return std::visit(
            overload{
                [](EdgeInfo::Require const& edge) { return "requires " + edge.m_dep.str(); },
                [](EdgeInfo::Constraint const& edge) { return "constraint " + edge.m_dep.str(); },
            },
            m_info);
    }

    template <class T, class U>
    auto MProblemsGraph<T, U>::graph() -> graph_t&
    {
        return m_graph;
    }

    template <class T, class U>
    void MProblemsGraph<T, U>::add_conflicts(node_id node1, node_id node2)
    {
        m_node_id_conflicts[node1].push_back(node2);
        m_node_id_conflicts[node2].push_back(node1);
    }

    template <class T, class U>
    auto MProblemsGraph<T, U>::get_conflicts() const -> node_id_conflicts const&
    {
        return m_node_id_conflicts;
    }

}