// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

extern "C"
{
#include <solv/pool.h>
}

#include "spdlog/spdlog.h"

#include "mamba/core/context.hpp"
#include "mamba/core/pool.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    void libsolv_debug_callback(Pool* /*pool*/, void* userptr, int type, const char* str)
    {
        auto* dbg = (std::pair<spdlog::logger*, std::string>*) (userptr);
        dbg->second += str;
        if (dbg->second.size() == 0 || dbg->second.back() != '\n')
        {
            return;
        }

        auto log = Console::hide_secrets(dbg->second);
        if (type & SOLV_FATAL || type & SOLV_ERROR)
        {
            dbg->first->error(log);
        }
        else if (type & SOLV_WARN)
        {
            dbg->first->warn(log);
        }
        else if (Context::instance().verbosity > 2)
        {
            dbg->first->info(log);
        }
        dbg->second.clear();
    }

    MPool::MPool()
    {
        m_pool = pool_create();
        pool_setdisttype(m_pool, DISTTYPE_CONDA);
        set_debuglevel();
    }

    MPool::~MPool()
    {
        LOG_INFO << "Freeing pool.";
        m_repo_list.clear();
        pool_free(m_pool);
    }

    void MPool::set_debuglevel()
    {
        // ensure that debug logging goes to stderr as to not interfere with stdout json output
        m_pool->debugmask |= SOLV_DEBUG_TO_STDERR;
        if (Context::instance().verbosity > 2)
        {
            pool_setdebuglevel(m_pool, Context::instance().verbosity - 1);
            auto logger = spdlog::get("libsolv");
            m_debug_logger.first = logger.get();
            pool_setdebugcallback(m_pool, &libsolv_debug_callback, &m_debug_logger);
        }
    }

    void MPool::create_whatprovides()
    {
        pool_createwhatprovides(m_pool);
    }

    MPool::operator Pool*()
    {
        return m_pool;
    }

    MRepo& MPool::add_repo(MRepo&& repo)
    {
        m_repo_list.push_back(std::move(repo));
        return m_repo_list.back();
    }

    void MPool::remove_repo(Id repo_id)
    {
        m_repo_list.remove_if([repo_id](const MRepo& repo) { return repo_id == repo.id(); });
    }

}  // namespace mamba
