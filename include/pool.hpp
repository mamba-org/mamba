#pragma once

#include "thirdparty/minilog.hpp"

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
        }

        ~MPool()
        {
            LOG(INFO) << "Freeing pool.";
            pool_free(m_pool);
        }

        void set_debuglevel(int lvl)
        {
            if (lvl == 0)
            {
                minilog::global_log_severity = 3;
            }
            else if (lvl == 1)
            {
                minilog::global_log_severity = 1;
            }
            else 
            {
                minilog::global_log_severity = 0;
            }
            pool_setdebuglevel(m_pool, lvl);
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