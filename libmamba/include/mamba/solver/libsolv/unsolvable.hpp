// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_LIBSOLV_UNSOLVABLE_HPP
#define MAMBA_SOLVER_LIBSOLV_UNSOLVABLE_HPP

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

#include "mamba/solver/problems_graph.hpp"

namespace solv
{
    class ObjSolver;
}

namespace mamba
{
    class Palette;
}

namespace mamba::solver::libsolv
{
    class Solver;
    class Database;

    class UnSolvable
    {
    public:

        UnSolvable(UnSolvable&&);

        ~UnSolvable();

        auto operator=(UnSolvable&&) -> UnSolvable&;

        [[nodiscard]] auto problems(Database& pool) const -> std::vector<std::string>;

        [[nodiscard]] auto problems_to_str(Database& pool) const -> std::string;

        [[nodiscard]] auto all_problems_to_str(Database& pool) const -> std::string;

        [[nodiscard]] auto problems_graph(const Database& pool) const -> ProblemsGraph;

        auto explain_problems_to(  //
            Database& pool,
            std::ostream& out,
            const ProblemsMessageFormat& format
        ) const -> std::ostream&;

        [[nodiscard]] auto
        explain_problems(Database& pool, const ProblemsMessageFormat& format) const -> std::string;

    private:

        // Pimpl all libsolv to keep it private
        // We could make it a reference if we consider it is worth keeping the data in the Solver
        // for potential resolve.
        std::unique_ptr<solv::ObjSolver> m_solver;

        explicit UnSolvable(std::unique_ptr<solv::ObjSolver>&& solver);

        [[nodiscard]] auto solver() const -> const solv::ObjSolver&;

        friend class Solver;
    };
}
#endif
