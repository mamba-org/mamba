// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_HPP
#define MAMBA_CORE_POOL_HPP

#include <optional>
#include <memory>

#include <solv/pooltypes.h>

#include "mamba/core/repo.hpp"
#include "mamba/core/package_info.hpp"

namespace mamba
{
    class MPool
    {
    public:
        MPool();
        ~MPool();

        void set_debuglevel();
        void create_whatprovides();

        std::vector<Id> select_solvables(Id id, bool sorted = false) const;
        Id matchspec2id(const std::string& ms);

        std::optional<PackageInfo> id2pkginfo(Id id);

        operator Pool*();
        operator Pool const*() const;

        MRepo& add_repo(MRepo&& repo);
        void remove_repo(Id repo_id);

    private:
        struct MPoolData;

        Pool* pool();
        Pool const* pool() const;

        /**
         * Make MPool behave like a shared_ptr (with move and copy).
         *
         * The pool is passed to the ``MSolver`` for its lifetime but the pool can legitimely
         * be reused with different solvers (in fact it is in ``conda-forge``'s ``regro-bot``
         * and ``boa``).
         * An alternative considered was to make ``MPool`` a move-only type, moving it in and out
         * of ``MSolver`` (effectively borrowing).
         * It was decided to make ``MPool`` share ressources for the following reasons.
         *    - Rvalue semantics would be unexpected in Python (and breaking ``conda-forge``);
         *    - Facilitate (potential) future investigation of parallel solves.
         */
        std::shared_ptr<MPoolData> m_data;
    };
}  // namespace mamba

#endif  // MAMBA_POOL_HPP
