// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/pool.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"

namespace mamba
{
    void libsolv_debug_callback(Pool* pool, void* userptr, int type, const char* str)
    {
        auto* logger = (spdlog::logger*)(userptr);
        std::string s(str);
        if (strip(s).size() == 0) return;
        if (s.back() != '\n') s.push_back('\n');
        if (type & SOLV_FATAL || type & SOLV_ERROR)
        {
            logger->error(s);
        }
        else if (type & SOLV_WARN)
        {
            logger->warn(s);
        }
        else if (Context::instance().verbosity > 2)
        {
            logger->info(s);
        }
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
            pool_setdebugcallback(m_pool, &libsolv_debug_callback, logger.get());
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
}  // namespace mamba
