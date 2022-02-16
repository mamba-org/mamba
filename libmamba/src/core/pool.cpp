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
        // if (Context::instance().verbosity > 2)
        // {
        pool_setdebuglevel(m_pool, Context::instance().verbosity - 1);
        pool_setdebugcallback(m_pool, &MPool::debug_callback, this);
        // }
    }

    void MPool::debug_callback(Pool* pool, void* self, int type, const char* str)
    {
        // TODO figure out if we can use a custom formatter for SOLV
        // auto f = std::make_unique<spdlog::pattern_formatter>("%l %v", spdlog::pattern_time_type::local, std::string(""));  // disable eol
        // p_nmea_logger->set_formatter( std::move(f) );

        auto stripped = strip(str);
        if (stripped.size() == 0) return;

        if (type & SOLV_FATAL || type & SOLV_ERROR)
        {
            spdlog::error(strip(str));
        }
        else if (type & SOLV_WARN)
        {
            spdlog::warn(strip(str));
        }
        else if (Context::instance().verbosity > 2)
        {
            spdlog::info(strip(str));
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
