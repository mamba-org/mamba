// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_QUEUE_HPP
#define MAMBA_CORE_QUEUE_HPP

#include <vector>

extern "C"
{
#include "solv/queue.h"
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

            if (!p_queue)
            {
                throw std::runtime_error("libsolv error: could not initialize Queue");
            }
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

        int count()
        {
            return p_queue->count;
        }

        void clear()
        {
            queue_empty(p_queue);
        }

        Id& operator[](int idx)
        {
            return p_queue->elements[idx];
        }

        const Id& operator[](int idx) const
        {
            return p_queue->elements[idx];
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

        template <template <typename, typename...> class C>
        C<Id> as()
        {
            return C<Id>(begin(), end());
        }

    private:

        Queue* p_queue;
    };
}  // namespace mamba

#endif  // MAMBA_POOL_HPP
