// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <sstream>
#include <string>
#include <vector>

#include "mamba/core/output.hpp"
#include "mamba/solver/problems_graph.hpp"
#include "mamba/solver/resolvo/unsolvable.hpp"

namespace mamba::solver::resolvo
{
    UnSolvable::UnSolvable(std::string reason)
        : m_reason(std::move(reason))
    {
    }

    UnSolvable::UnSolvable(UnSolvable&&) = default;

    UnSolvable::~UnSolvable() = default;

    auto UnSolvable::operator=(UnSolvable&&) -> UnSolvable& = default;

    auto UnSolvable::problems(PackageDatabase& db) const -> std::vector<std::string>
    {
        // TODO: Implement
        return {};
    }

    auto UnSolvable::problems_to_str(PackageDatabase& db) const -> std::string
    {
        // TODO: Implement
        return {};
    }

    auto UnSolvable::all_problems_to_str(PackageDatabase& db) const -> std::string
    {
        // TODO: Implement
        return {};
    }

    auto UnSolvable::problems_graph(const PackageDatabase& pool) const -> ProblemsGraph
    {
        // TODO: Implement
        ProblemsGraph::graph_t graph;
        ProblemsGraph::conflicts_t conflicts;
        ProblemsGraph::node_id root_node;
        return ProblemsGraph{ graph, conflicts, root_node };
    }

    auto UnSolvable::explain_problems_to(
        PackageDatabase& pool,
        std::ostream& out,
        const ProblemsMessageFormat& format
    ) const -> std::ostream&
    {
        out << "Could not solve for environment specs\n";
        const auto pbs = problems_graph(pool);
        const auto pbs_simplified = simplify_conflicts(pbs);
        const auto cp_pbs = CompressedProblemsGraph::from_problems_graph(pbs_simplified);
        print_problem_tree_msg(out, cp_pbs, format);
        return out;
    }

    auto UnSolvable::explain_problems(PackageDatabase& pool, const ProblemsMessageFormat& format) const
        -> std::string

    {
        std::stringstream ss;
        explain_problems_to(pool, ss, format);
        return ss.str();
    }
}
