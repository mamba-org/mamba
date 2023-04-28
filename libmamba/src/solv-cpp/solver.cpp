// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>

#include <solv/poolid.h>
#include <solv/solver.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"
#include "solv-cpp/solver.hpp"

namespace mamba::solv
{
    void ObjSolver::SolverDeleter::operator()(::Solver* ptr)
    {
        ::solver_free(ptr);
    }

    ObjSolver::ObjSolver(const ObjPool& pool)
        : m_solver(::solver_create(const_cast<::Pool*>(pool.raw())))
    {
    }

    ObjSolver::~ObjSolver() = default;

    auto ObjSolver::raw() -> ::Solver*
    {
        return m_solver.get();
    }

    void ObjSolver::set_flag(SolverFlag flag, bool value)
    {
        ::solver_set_flag(raw(), flag, value);
    }

    auto ObjSolver::get_flag(SolverFlag flag) const -> bool
    {
        const auto val = ::solver_get_flag(const_cast<::Solver*>(raw()), flag);
        assert((val == 0) || (val == 1));
        return val != 0;
    }

    auto ObjSolver::raw() const -> const ::Solver*
    {
        return m_solver.get();
    }

    auto ObjSolver::problem_count() const -> std::size_t
    {
        return ::solver_problem_count(const_cast<::Solver*>(raw()));
    }

    auto ObjSolver::solve(const ObjPool& /* pool */, const ObjQueue& jobs) -> bool
    {
        // pool is captured inside solver so we take it as a parameter to be explicit.
        const auto n_pbs = ::solver_solve(raw(), const_cast<::Queue*>(jobs.raw()));
        return n_pbs == 0;
    }
}
