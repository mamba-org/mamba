// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <cassert>

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

    namespace
    {
        void
        assert_same_pool([[maybe_unused]] const ObjPool& pool, [[maybe_unused]] const ObjTransaction& trans)
        {
            assert(pool.raw() == trans.raw()->pool);
        }
    }

    auto ObjTransaction::from_solver(const ObjPool& pool, const ObjSolver& solver) -> ObjTransaction
    {
        auto trans = ObjTransaction{ ::solver_create_transaction(const_cast<::Solver*>(solver.raw())
        ) };
        assert_same_pool(pool, trans);
        return trans;
    }

    auto ObjTransaction::raw() -> ::Transaction*
    {
        return m_transaction.get();
    }

    auto ObjTransaction::raw() const -> const ::Transaction*
    {
        return m_transaction.get();
    }

    auto ObjTransaction::empty() const -> bool
    {
        return raw()->steps.count <= 0;
    }

    auto ObjTransaction::size() const -> std::size_t
    {
        assert(raw()->steps.count >= 0);
        return static_cast<std::size_t>(raw()->steps.count);
    }

    auto ObjTransaction::steps() const -> ObjQueue
    {
        ObjQueue out = {};
        for_each_step_id([&](auto id) { out.push_back(id); });
        return out;
    }

    auto ObjTransaction::step_type(const ObjPool& pool, SolvableId step, TransactionMode mode) const
        -> TransactionStepType
    {
        assert_same_pool(pool, *this);
        return ::transaction_type(const_cast<::Transaction*>(raw()), step, mode);
    }

    auto ObjTransaction::step_newer(const ObjPool& pool, SolvableId step) const
        -> std::optional<SolvableId>
    {
        assert_same_pool(pool, *this);
        if (const auto solvable = pool.get_solvable(step); solvable && solvable->installed())
        {
            if (auto id = ::transaction_obs_pkg(const_cast<::Transaction*>(raw()), step); id != 0)
            {
                return { id };
            }
        }
        return std::nullopt;
    }

    auto ObjTransaction::step_olders(const ObjPool& pool, SolvableId step) const -> ObjQueue
    {
        assert_same_pool(pool, *this);
        auto out = ObjQueue{};
        if (const auto solvable = pool.get_solvable(step); solvable && !solvable->installed())
        {
            ::transaction_all_obs_pkgs(const_cast<::Transaction*>(raw()), step, out.raw());
        }
        return out;
    }

    void ObjTransaction::order(const ObjPool& pool, TransactionOrderFlag flag)
    {
        assert_same_pool(pool, *this);
        ::transaction_order(raw(), flag);
    }

    auto ObjTransaction::classify(const ObjPool& pool, TransactionMode mode) const -> ObjQueue
    {
        assert_same_pool(pool, *this);
        auto out = ObjQueue{};
        ::transaction_classify(const_cast<::Transaction*>(raw()), mode, out.raw());
        return out;
    }

    auto ObjTransaction::classify_pkgs(
        const ObjPool& pool,
        TransactionStepType type,
        StringId from,
        StringId to,
        TransactionMode mode
    ) const -> ObjQueue
    {
        assert_same_pool(pool, *this);
        auto out = ObjQueue{};
        ::transaction_classify_pkgs(const_cast<::Transaction*>(raw()), mode, type, from, to, out.raw());
        return out;
    }

}
