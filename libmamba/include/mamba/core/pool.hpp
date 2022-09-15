// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_HPP
#define MAMBA_CORE_POOL_HPP

#include <list>
#include <optional>

#include "mamba/core/repo.hpp"
#include "mamba/core/package_info.hpp"

extern "C"
{
#include "solv/pooltypes.h"
}

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
        ~MPool();

        MPool(const MPool&) = delete;
        MPool& operator=(const MPool&) = delete;
        MPool(MPool&&) = delete;
        MPool& operator=(MPool&&) = delete;

        void set_debuglevel();
        void create_whatprovides();

        std::vector<Id> select_solvables(Id id) const;
        Id matchspec2id(const std::string& ms);

        std::optional<PackageInfo> id2pkginfo(Id id);

        operator Pool*();

        MRepo& add_repo(MRepo&& repo);
        void remove_repo(Id repo_id);

    private:
        std::pair<spdlog::logger*, std::string> m_debug_logger;
        Pool* m_pool;
        std::list<MRepo> m_repo_list;
    };
}  // namespace mamba

#endif  // MAMBA_POOL_HPP
