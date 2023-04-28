// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_SOLVER_HPP
#define MAMBA_SOLV_SOLVER_HPP

#include <memory>

#include "solv-cpp/ids.hpp"

using Solver = struct s_Solver;

namespace mamba::solv
{
    class ObjPool;
    class ObjQueue;

    class ObjSolver
    {
    public:

        ObjSolver(const ObjPool& pool);
        ~ObjSolver();

        auto raw() -> ::Solver*;
        auto raw() const -> const ::Solver*;

        void set_flag(SolverFlag flag, bool value);
        [[nodiscard]] auto get_flag(SolverFlag flag) const -> bool;

        [[nodiscard]] auto solve(const ObjPool& pool, const ObjQueue& jobs) -> bool;
        [[nodiscard]] auto problem_count() const -> std::size_t;

    private:

        struct SolverDeleter
        {
            void operator()(::Solver* ptr);
        };

        std::unique_ptr<::Solver, ObjSolver::SolverDeleter> m_solver = nullptr;
    };
}

#endif
