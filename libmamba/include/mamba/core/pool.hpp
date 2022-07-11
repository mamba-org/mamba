// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_POOL_HPP
#define MAMBA_CORE_POOL_HPP

#include <list>
#include "repo.hpp"

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
    class MQueue
    {
    public:
        MQueue()
            : p_queue(new Queue)
        {
            queue_init(p_queue);
        }

        ~MQueue()
        {
            queue_free(p_queue);
        }

        void push(Id id)
        {
            queue_push(p_queue, id);
        }

        void push(Id id1, Id id2)
        {
            queue_push2(p_queue, id1, id2);
        }

        Id* begin()
        {
            return &p_queue->elements[0];
        }

        Id* end()
        {
            return &p_queue->elements[p_queue->count];
        }

        const Id* begin() const
        {
            return &p_queue->elements[0];
        }

        const Id* end() const
        {
            return &p_queue->elements[p_queue->count];
        }

        operator Queue*()
        {
            return p_queue;
        }

        std::vector<Id> as_vector()
        {
            return std::vector<Id>(begin(), end());
        }

    private:
        Queue* p_queue;
    };

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

        std::vector<Id> select_solvables(Id id);
        Id matchspec2id(const std::string& ms);

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
