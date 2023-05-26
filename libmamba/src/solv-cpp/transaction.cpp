// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <solv/solver.h>
#include <solv/transaction.h>

#include "solv-cpp/pool.hpp"
#include "solv-cpp/solver.hpp"
#include "solv-cpp/transaction.hpp"

namespace mamba::solv
{
    void ObjTransaction::TransactionDeleter::operator()(::Transaction* ptr)
    {
        ::transaction_free(ptr);
    }

    ObjTransaction::ObjTransaction(const ObjPool& pool)
        : ObjTransaction(::transaction_create(const_cast<::Pool*>(pool.raw())))
    {
    }

    ObjTransaction::ObjTransaction(::Transaction* ptr) noexcept
        : m_transaction(ptr)
    {
    }

    ObjTransaction::ObjTransaction(const ObjTransaction& other)
        : ObjTransaction(::transaction_create_clone(const_cast<::Transaction*>(other.raw())))
    {
    }

    ObjTransaction::~ObjTransaction() = default;

    auto ObjTransaction::operator=(const ObjTransaction& other) -> ObjTransaction&
    {
        *this = ObjTransaction(other);
        return *this;
    }

    auto ObjTransaction::from_solvables(const ObjPool& pool, const ObjQueue& solvables)
        -> ObjTransaction
    {
        return ObjTransaction{ ::transaction_create_decisionq(
            const_cast<::Pool*>(pool.raw()),
            const_cast<::Queue*>(solvables.raw()),
            nullptr
        ) };
    }

    auto ObjTransaction::from_solver(const ObjPool& /*pool*/, const ObjSolver& solver)
        -> ObjTransaction
    {
        return ObjTransaction{ ::solver_create_transaction(const_cast<::Solver*>(solver.raw())) };
    }

    auto ObjTransaction::raw() -> ::Transaction*
    {
        return m_transaction.get();
    }

    auto ObjTransaction::raw() const -> const ::Transaction*
    {
        return m_transaction.get();
    }
}
