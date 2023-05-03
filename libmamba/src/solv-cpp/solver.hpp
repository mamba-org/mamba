// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_SOLVER_HPP
#define MAMBA_SOLV_SOLVER_HPP

#include <memory>
#include <string>

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
        [[nodiscard]] auto problem_to_string(const ObjPool& pool, ProblemId id) const -> std::string;
        template <typename UnaryFunc>
        void for_each_problem_id(UnaryFunc&& func) const;

    private:

        struct SolverDeleter
        {
            void operator()(::Solver* ptr);
        };

        std::unique_ptr<::Solver, ObjSolver::SolverDeleter> m_solver = nullptr;

        auto next_problem(ProblemId id = 0) const -> ProblemId;
    };

    /*********************************
     *  Implementation of ObjSolver  *
     *********************************/

    template <typename UnaryFunc>
    void ObjSolver::for_each_problem_id(UnaryFunc&& func) const
    {
        for (ProblemId id = next_problem(); id != 0; id = next_problem(id))
        {
            func(id);
        }
    }

}
#endif
