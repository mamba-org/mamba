// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_PROBLEMS_GRAPH_CREATOR_HPP
#define MAMBA_PROBLEMS_GRAPH_CREATOR_HPP

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
#include "mamba/core/problems_graph.hpp"
#include "mamba/core/package_info.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/util.hpp"

extern "C"
{
#include "solv/solver.h"
}

namespace mamba
{

    class MProblemsGraphCreator
    {
    public:
        using graph = MProblemsGraph<MNode, MEdgeInfo>;

        MProblemsGraphCreator(MPool* pool);
        
        const graph& create_graph_from_problems(const std::vector<MSolverProblem>& problems,
                                                const std::vector<std::string>& initial_specs);

    private:
        MPool* m_pool;
        graph m_problems_graph;

        void add_problem_to_graph(const MSolverProblem& problem,
                                  const std::vector<std::string>& initial_specs);
    };

    inline std::string get_value_or(const std::optional<PackageInfo>& pkg_info)
    {
        if (pkg_info)
        {
            return pkg_info.value().str();
        }
        else
        {
            return "(null)";
        }
    }

    template <typename T>
    inline bool has_values(const MSolverProblem& problem,
                           std::initializer_list<std::optional<T>> args)
    {
        for (const auto& e : args)
        {
            if (!e)
            {
                LOG_WARNING << "Unexpected empty optionals for problem " << problem.to_string()
                            << "source: " << get_value_or(problem.source())
                            << "target: " << get_value_or(problem.target())
                            << "dep: " << problem.dep().value_or("(null)") << std::endl;
                return false;
            }
        }
        return true;
    }

    inline bool contains_any_substring(std::string str, const std::vector<std::string>& substrings)
    {
        for (const auto& substring : substrings)
        {
            if (str.find(substring) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }
}

#endif  // MAMBA_PROBLEMS_GRAPH_CREATOR_HPP