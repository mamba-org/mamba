// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_HPP
#define MAMBA_CORE_POOL_HPP

#include <list>
#include <optional>
#include <memory>

#include <solv/pooltypes.h>

#include "mamba/core/repo.hpp"
#include "mamba/core/package_info.hpp"

namespace spdlog
{
    class logger;
}

namespace mamba
{
    class MPool
    {
    public:
        MPool();
        ~MPool() = default;

        MPool(const MPool&) = delete;
        MPool& operator=(const MPool&) = delete;
        MPool(MPool&&) = default;
        MPool& operator=(MPool&&) = default;

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
        static void delete_libsolv_pool(::Pool* pool);

        std::pair<spdlog::logger*, std::string> m_debug_logger;
        std::unique_ptr<Pool, decltype(&MPool::delete_libsolv_pool)> m_pool;
        std::list<MRepo> m_repo_list;
    };
}  // namespace mamba

#endif  // MAMBA_POOL_HPP
