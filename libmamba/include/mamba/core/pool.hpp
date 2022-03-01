// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_HPP
#define MAMBA_CORE_POOL_HPP

#include "context.hpp"
#include "repo.hpp"

extern "C"
{
#include "solv/pool.h"
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

        operator Pool*();

        MRepo& add_repo(MRepo&& repo);

    private:
        std::pair<spdlog::logger*, std::string> m_debug_logger;
        Pool* m_pool;
        std::vector<MRepo> m_repo_list;
    };
}  // namespace mamba

#endif  // MAMBA_POOL_HPP
