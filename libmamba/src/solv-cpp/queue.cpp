// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include "solv-cpp/queue.hpp"

#include <cassert>
#include <limits>

#include <solv/queue.h>

namespace mamba::solv
{
    ObjQueue::ObjQueue(std::nullptr_t)
        : m_queue{}
    {
    }

    ObjQueue::ObjQueue()
        : ObjQueue(nullptr)
    {
        queue_init(get());
    }

    ObjQueue::ObjQueue(std::initializer_list<value_type> elems)
        : ObjQueue(elems.begin(), elems.end())
    {
    }

    ObjQueue::ObjQueue(ObjQueue&& other)
        : ObjQueue(nullptr)
    {
        swap(*this, other);
    }

    ObjQueue::ObjQueue(const ObjQueue& other)
        : ObjQueue(other.begin(), other.end())
    {
    }

    ObjQueue::~ObjQueue()
    {
        if (data() != nullptr)
        {
            queue_free(get());
        }
    }

    auto ObjQueue::operator=(ObjQueue&& other) -> ObjQueue&
    {
        swap(*this, other);
        // Leaving other empty to make sure ressources are no longer used
        auto empty = ObjQueue(nullptr);
        swap(other, empty);
        return *this;
    }

    auto ObjQueue::operator=(const ObjQueue& other) -> ObjQueue&
    {
        auto other_copy = ObjQueue(other);
        swap(*this, other_copy);
        return *this;
    }

    void ObjQueue::push_back(value_type id)
    {
        queue_push(get(), id);
    }

    void ObjQueue::push_back(value_type id1, value_type id2)
    {
        queue_push2(get(), id1, id2);
    }

    auto ObjQueue::insert(const_iterator pos, value_type id) -> iterator
    {
        const difference_type offset = pos - begin();
        assert(offset >= 0);
        assert(offset <= std::numeric_limits<int>::max());
        queue_insert(get(), static_cast<int>(offset), id);
        return begin() + offset;
    }

    auto ObjQueue::capacity() const -> size_type
    {
        return static_cast<size_type>(m_queue.count + m_queue.left);
    }

    auto ObjQueue::size() const -> size_type
    {
        assert(m_queue.count >= 0);
        return static_cast<std::size_t>(m_queue.count);
    }

    auto ObjQueue::empty() const -> bool
    {
        return size() == 0;
    }

    auto ObjQueue::erase(const_iterator pos) -> iterator
    {
        const difference_type offset = pos - begin();
        assert(offset >= 0);
        assert(offset <= std::numeric_limits<int>::max());
        queue_delete(get(), static_cast<int>(offset));
        return begin() + offset;
    }

    void ObjQueue::reserve(size_type new_cap)
    {
        if (new_cap < capacity())
        {
            return;
        }
        size_type cap_diff = new_cap - capacity();
        assert(cap_diff <= std::numeric_limits<int>::max());
        queue_prealloc(get(), static_cast<int>(cap_diff));
    }

    void ObjQueue::clear()
    {
        queue_empty(get());
    }

    auto ObjQueue::front() -> reference
    {
        return *begin();
    }

    auto ObjQueue::front() const -> const_reference
    {
        return *begin();
    }

    auto ObjQueue::back() -> reference
    {
        return *rbegin();
    }

    auto ObjQueue::back() const -> const_reference
    {
        return *rbegin();
    }

    auto ObjQueue::operator[](int idx) -> reference
    {
        return data()[idx];
    }

    auto ObjQueue::operator[](int idx) const -> const_reference
    {
        return data()[idx];
    }

    auto ObjQueue::begin() -> iterator
    {
        return data();
    }

    auto ObjQueue::begin() const -> const_iterator
    {
        return data();
    }

    auto ObjQueue::end() -> iterator
    {
        return data() + size();
    }

    auto ObjQueue::end() const -> const_iterator
    {
        return data() + size();
    }

    auto ObjQueue::rbegin() -> reverse_iterator
    {
        return std::reverse_iterator{ end() };
    }

    auto ObjQueue::rbegin() const -> const_reverse_iterator
    {
        return std::reverse_iterator{ end() };
    }

    auto ObjQueue::rend() -> reverse_iterator
    {
        return std::reverse_iterator{ begin() };
    }

    auto ObjQueue::rend() const -> const_reverse_iterator
    {
        return std::reverse_iterator{ begin() };
    }

    auto ObjQueue::data() -> pointer
    {
        return m_queue.elements;
    }

    auto ObjQueue::data() const -> const_pointer
    {
        return m_queue.elements;
    }

    auto ObjQueue::get() const -> const ::Queue*
    {
        return &m_queue;
    }

    auto ObjQueue::get() -> ::Queue*
    {
        return &m_queue;
    }

    auto ObjQueue::offset_of(const_iterator pos) const -> size_type
    {
        const auto offset = std::distance(begin(), pos);
        assert(offset >= 0);
        return static_cast<std::size_t>(offset);
    }

    void ObjQueue::insert_n(size_type pos, const_iterator first, size_type n)
    {
        assert(pos <= std::numeric_limits<int>::max());
        assert(n <= std::numeric_limits<int>::max());
        queue_insertn(get(), static_cast<int>(pos), static_cast<int>(n), first);
    }

    void swap(ObjQueue& a, ObjQueue& b) noexcept
    {
        using std::swap;
        swap(a.m_queue, b.m_queue);
    }

}  // namespace mamba
