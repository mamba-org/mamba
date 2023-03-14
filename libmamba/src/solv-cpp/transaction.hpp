// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_TRANSACTION_HPP
#define MAMBA_SOLV_TRANSACTION_HPP

#include <memory>

extern "C"
{
    using Transaction = struct s_Transaction;
}

namespace mamba::solv
{
    class ObjPool;
    class ObjSolver;
    class ObjQueue;

    class ObjTransaction
    {
    public:

        [[nodiscard]] static auto from_solvables(const ObjPool& pool, const ObjQueue& solvables)
            -> ObjTransaction;
        [[nodiscard]] static auto from_solver(const ObjPool& pool, const ObjSolver& solver)
            -> ObjTransaction;

        ObjTransaction(const ObjPool& pool);
        ~ObjTransaction();

        [[nodiscard]] auto raw() -> ::Transaction*;
        [[nodiscard]] auto raw() const -> const ::Transaction*;

    private:

        struct TransactionDeleter
        {
            void operator()(::Transaction* ptr);
        };

        std::unique_ptr<::Transaction, ObjTransaction::TransactionDeleter> m_transaction;

        explicit ObjTransaction(::Transaction* ptr) noexcept;
    };
}
#endif
