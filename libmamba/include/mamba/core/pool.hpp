// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_HPP
#define MAMBA_CORE_POOL_HPP

#include <list>
#include <optional>
#include "repo.hpp"
#include "package_info.hpp"

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

    class IMPool
    {
    public:

        virtual ~IMPool() = 0;
        
        virtual void set_debuglevel() = 0;
        virtual void create_whatprovides() = 0;

        virtual std::vector<Id> select_solvables(Id id) = 0;
        virtual Id matchspec2id(const std::string& ms) = 0;

        virtual std::optional<PackageInfo> id2pkginfo(Id id) = 0;

        virtual operator Pool*() = 0;

        virtual MRepo& add_repo(MRepo&& repo) = 0;
        virtual void remove_repo(Id repo_id) = 0;
    };

    class MPool : public IMPool
    {
    public:
        MPool();
        ~MPool();

        MPool(const MPool&) = delete;
        MPool& operator=(const MPool&) = delete;
        MPool(MPool&&) = delete;
        MPool& operator=(MPool&&) = delete;

        void set_debuglevel() override;
        void create_whatprovides() override;

        std::vector<Id> select_solvables(Id id) override;
        Id matchspec2id(const std::string& ms) override;

        std::optional<PackageInfo> id2pkginfo(Id id) override;

        operator Pool*() override;

        MRepo& add_repo(MRepo&& repo) override;
        void remove_repo(Id repo_id) override;

    private:
        std::pair<spdlog::logger*, std::string> m_debug_logger;
        Pool* m_pool;
        std::list<MRepo> m_repo_list;
    };
}  // namespace mamba

#endif  // MAMBA_POOL_HPP
