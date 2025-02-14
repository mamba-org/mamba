// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#pragma once

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "mamba/solver/problems_graph.hpp"

namespace mamba
{
    class Palette;
}

namespace mamba::solver::resolvo
{
    class Solver;
    class PackageDatabase;

    class UnSolvable
    {
    public:

        UnSolvable(std::string reason);

        UnSolvable(UnSolvable&&);

        ~UnSolvable();

        auto operator=(UnSolvable&&) -> UnSolvable&;

        [[nodiscard]] auto problems(PackageDatabase& pool) const -> std::vector<std::string>;

        [[nodiscard]] auto problems_to_str(PackageDatabase& pool) const -> std::string;

        [[nodiscard]] auto all_problems_to_str(PackageDatabase& pool) const -> std::string;

        [[nodiscard]] auto problems_graph(const PackageDatabase& pool) const -> ProblemsGraph;

        auto explain_problems_to(  //
            PackageDatabase& pool,
            std::ostream& out,
            const ProblemsMessageFormat& format
        ) const -> std::ostream&;

        [[nodiscard]] auto
        explain_problems(PackageDatabase& pool, const ProblemsMessageFormat& format) const -> std::string;

    private:

        std::string m_reason;

        friend class Solver;
    };
}
