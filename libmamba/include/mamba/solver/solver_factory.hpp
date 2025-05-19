// Copyright (c) 2024, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLVER_SOLVER_FACTORY_HPP
#define MAMBA_SOLVER_SOLVER_FACTORY_HPP

#include <memory>
#include <variant>

#include "mamba/core/context.hpp"
#include "mamba/solver/libsolv/database.hpp"
#include "mamba/solver/libsolv/solver.hpp"
#include "mamba/solver/resolvo/database.hpp"
#include "mamba/solver/resolvo/solver.hpp"

namespace mamba::solver
{
    /**
     * Type alias for the database variant that can hold either libsolv or resolvo database.
     */
    using DatabaseVariant = std::variant<libsolv::Database, resolvo::Database>;

    /**
     * Create a solver based on the configuration.
     *
     * @param ctx The context containing the configuration.
     * @return A unique pointer to the appropriate solver.
     */
    template <typename Database>
    auto create_solver(const Context& ctx)
    {
        if (ctx.use_resolvo_solver)
        {
            return std::make_unique<resolvo::Solver>();
        }
        else
        {
            return std::make_unique<libsolv::Solver>();
        }
    }
}
#endif
