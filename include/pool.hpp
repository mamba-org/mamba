#pragma once

#include "thirdparty/minilog.hpp"
#include "context.hpp"

extern "C"
{
    #include "solv/pool.h"
}

namespace mamba
{
    class MPool
    {
    public:
        MPool()
        {
            m_pool = pool_create();
            pool_setdisttype(m_pool, DISTTYPE_CONDA);
            set_debuglevel();
        }

        ~MPool()
        {
            LOG(INFO) << "Freeing pool.";
            pool_free(m_pool);
        }

        void set_debuglevel()
        {
            pool_setdebuglevel(m_pool, Context::instance().verbosity);
        }

        void create_whatprovides()
        {
            pool_createwhatprovides(m_pool);
        }

        operator Pool*()
        {
            return m_pool;
        }

        Pool* m_pool;
    };
}