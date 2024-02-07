// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_TRANSACTION_HPP
#define MAMBA_SOLV_TRANSACTION_HPP

#include <memory>
#include <optional>
#include <utility>

#include <solv/transaction.h>

#include "solv-cpp/ids.hpp"
#include "solv-cpp/pool.hpp"
#include "solv-cpp/queue.hpp"

namespace solv
{
    class ObjPool;
    class ObjSolver;

    /**
     * Transactions describe the output of a solver run.
     *
     * A transaction contains a number of transaction steps, each either the installation of a
     * new package or the removal of an already installed package.
     */
    class ObjTransaction
    {
    public:

        /**
         * Create a transaction from a list of solvables to add/remove.
         *
         * Negative solvable ids are use to mean that the solvable must be removed.
         */
        [[nodiscard]] static auto from_solvables(const ObjPool& pool, const ObjQueue& solvables)
            -> ObjTransaction;

        /**
         * Create a transaction from the result of a solver run.
         *
         * The solver must be solved.
         */
        [[nodiscard]] static auto from_solver(const ObjPool& pool, const ObjSolver& solver)
            -> ObjTransaction;

        ObjTransaction(const ObjPool& pool);
        ObjTransaction(ObjTransaction&&) noexcept = default;
        ObjTransaction(const ObjTransaction&);
        auto operator=(ObjTransaction&&) noexcept -> ObjTransaction& = default;
        auto operator=(const ObjTransaction&) -> ObjTransaction&;

        [[nodiscard]] auto raw() -> ::Transaction*;
        [[nodiscard]] auto raw() const -> const ::Transaction*;

        /** Whether the transaction contains any step. */
        [[nodiscard]] auto empty() const -> bool;

        /** Number of steps in the transaction. */
        [[nodiscard]] auto size() const -> std::size_t;

        /**
         * Return a copy of the steps.
         *
         * The steps are the action resulting from, for instance, the solver run and that
         * need to be performed.
         */
        // TODO C++20 We can simply return a ``span<solv::SolvableId>`` to replace both functions
        auto steps() const -> ObjQueue;
        template <typename UnaryFunc>
        void for_each_step_id(UnaryFunc&& func) const;
        template <typename UnaryFunc>
        void for_each_step_solvable(const ObjPool& pool, UnaryFunc&& func) const;

        /** The type of the step, such as install, remove etc. */
        auto step_type(const ObjPool& pool, SolvableId step, TransactionMode mode = 0) const
            -> TransactionStepType;

        /**
         * Classify the transaction steps.
         *
         * Iterate through a sorted collection of steps types along with the solvables ids in that
         * step.
         * All steps do not appear as part of a type solvables ids pair.
         * The user need to query @ref step_newer (or @step_olders?) to get all solvables.
         * This is useful to write summarry message and connect solvables together (e.g. solvable
         * A1 (removed) is ugraded by solvable A2 (added)) while avoiding duplicates.
         */
        template <typename BinaryFunc>
        void
        classify_for_each_type(const ObjPool& pool, BinaryFunc&& func, TransactionMode mode = 0) const;

        /**
         * Return the solvable that replace the one in the given step.
         *
         * For an installed solvable appearing in the transaction, return the potential
         * solvable id that replace this one.
         */
        auto step_newer(const ObjPool& pool, SolvableId step) const -> std::optional<SolvableId>;

        /**
         * Return the solvables that are replaced by the one in the given step.
         *
         * For a not installed solvable appearing in the transaction, return the potential
         * solvable id that are replaced by this one.
         */
        auto step_olders(const ObjPool& pool, SolvableId step) const -> ObjQueue;

        /**
         * Topological sort of the packages in the transaction.
         *
         * Order the steps in the transactions so that dependent packages are updated before
         * packages that depend on them.
         */
        void order(const ObjPool& pool, TransactionOrderFlag flag = 0);


    private:

        struct TransactionDeleter
        {
            void operator()(::Transaction* ptr);
        };

        std::unique_ptr<::Transaction, ObjTransaction::TransactionDeleter> m_transaction;

        explicit ObjTransaction(::Transaction* ptr) noexcept;

        auto classify(const ObjPool& pool, TransactionMode mode = 0) const -> ObjQueue;

        auto classify_pkgs(
            const ObjPool& pool,
            TransactionStepType type,
            StringId from,
            StringId to,
            TransactionMode mode = 0
        ) const -> ObjQueue;
    };

    /**************************************
     *  Implementation of ObjTransaction  *
     **************************************/

    template <typename UnaryFunc>
    void ObjTransaction::for_each_step_id(UnaryFunc&& func) const
    {
        const auto& steps = raw()->steps;
        const auto count = static_cast<std::size_t>(steps.count);
        for (std::size_t i = 0; i < count; ++i)
        {
            const auto id = steps.elements[i];
            if constexpr (std::is_same_v<decltype(func(id)), LoopControl>)
            {
                if (func(id) == LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(id);
            }
        }
    }

    template <typename UnaryFunc>
    void ObjTransaction::for_each_step_solvable(const ObjPool& pool, UnaryFunc&& func) const
    {
        for_each_step_id([&](const auto id) { func(pool.get_solvable(id)); });
    }

    template <typename BinaryFunc>
    void
    ObjTransaction::classify_for_each_type(const ObjPool& pool, BinaryFunc&& func, TransactionMode mode) const
    {
        const auto types = classify(pool, mode);
        // By four of [type, count, from?, to?]
        for (std::size_t n = types.size(), i = 0; i < n; i += 4)
        {
            const TransactionStepType type = types[i];
            auto ids = classify_pkgs(pool, type, types[i + 2], types[i + 3], mode);
            if constexpr (std::is_same_v<decltype(func(type, std::move(ids))), LoopControl>)
            {
                if (func(type, std::move(ids)) == LoopControl::Break)
                {
                    break;
                }
            }
            else
            {
                func(type, std::move(ids));
            }
        }
    }
}

#endif
