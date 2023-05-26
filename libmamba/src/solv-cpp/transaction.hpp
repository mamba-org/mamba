// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_TRANSACTION_HPP
#define MAMBA_SOLV_TRANSACTION_HPP

#include <memory>
#include <utility>

#include "solv-cpp/ids.hpp"

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
        ObjTransaction(ObjTransaction&&) noexcept = default;
        ObjTransaction(const ObjTransaction&);
        ~ObjTransaction();
        auto operator=(ObjTransaction&&) noexcept -> ObjTransaction& = default;
        auto operator=(const ObjTransaction&) -> ObjTransaction&;

        [[nodiscard]] auto raw() -> ::Transaction*;
        [[nodiscard]] auto raw() const -> const ::Transaction*;

        // TODO C++20 We can simply return a ``span<solv::SolvableId>``
        template <typename UnaryFunc>
        void for_each_step_id(UnaryFunc&& func) const;

    private:

        struct TransactionDeleter
        {
            void operator()(::Transaction* ptr);
        };

        std::unique_ptr<::Transaction, ObjTransaction::TransactionDeleter> m_transaction;

        explicit ObjTransaction(::Transaction* ptr) noexcept;
    };
}

#include <solv/transaction.h>

namespace mamba::solv
{
    template <typename UnaryFunc>
    void ObjTransaction::for_each_step_id(UnaryFunc&& func) const
    {
        const auto& steps = raw()->steps;
        const auto count = static_cast<std::size_t>(steps.count);
        for (std::size_t i = 0; i < count; ++i)
        {
            func(steps.elements[i]);
        }
    }
}

#endif
