#ifndef MAMBA_POOL_HPP
#define MAMBA_POOL_HPP

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

        MPool();
        ~MPool();

        MPool(const MPool&) = delete;
        MPool& operator=(const MPool&) = delete;
        MPool(MPool&&) = delete;
        MPool& operator=(MPool&&) = delete;

        void set_debuglevel();
        void create_whatprovides();

        operator Pool*();

    private:

        Pool* m_pool;
    };
}

#endif // MAMBA_POOL_HPP
