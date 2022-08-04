// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_WRAPPER_HPP
#define MAMBA_CORE_POOL_WRAPPER_HPP

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
    template <typename PoolImpl>
    class MPoolWrapper {
    public:
        //TODO move to virtual or extend this with the remaning methods
        MPoolWrapper(PoolImpl* poolImpl): _poolImpl(poolImpl) {}

        std::vector<Id> select_solvables(Id id)
        {
            return (*_poolImpl)->select_solvables(id);
        }

        operator Pool*()
        {
            return **_poolImpl;
        }

    private:
        PoolImpl* _poolImpl;
    };
}

#endif 
