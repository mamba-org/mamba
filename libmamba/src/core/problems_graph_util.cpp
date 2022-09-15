// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>

#include "mamba/core/problems_graph_util.hpp"

namespace mamba
{
    DependencyInfo::DependencyInfo(const std::string& dep)
    {
        static std::regex const regexp("\\s*(\\w[\\w-]*)\\s*([^\\s]*)\\s*");
        std::smatch matches;
        bool const matched = std::regex_match(dep, matches, regexp);
        // First match is the whole regex match
        if (!matched || matches.size() != 3)
        {
            throw std::runtime_error("Invalid dependency info: " + dep);
        }
        m_name = matches.str(1);
        m_range = matches.str(2);
    }

    std::string const& DependencyInfo::name() const
    {
        return m_name;
    }

    std::string const& DependencyInfo::range() const
    {
        return m_range;
    }

    std::string DependencyInfo::str() const
    {
        return m_name + " " + m_range;
    }

    auto ProblemsGraph::graph() const noexcept -> graph_t const&
    {
        return m_graph;
    }

    void ProblemsGraph::add_conflicts(node_id node1, node_id node2)
    {
        m_node_id_conflicts[node1].insert(node2);
        m_node_id_conflicts[node2].insert(node1);
    }

    auto ProblemsGraph::conflicts() const noexcept -> conflict_map const&
    {
        return m_node_id_conflicts;
    }
}
