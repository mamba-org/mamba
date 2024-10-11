// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <algorithm>
#include <cassert>
#include <limits>
#include <sstream>

#include <solv/queue.h>

#include "solv-cpp/queue.hpp"

namespace solv
{
    ObjQueue::ObjQueue(std::nullptr_t)
    {
    }

    ObjQueue::ObjQueue()
        : ObjQueue(nullptr)
    {
        queue_init(raw());
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
            queue_free(raw());
        }
    }

    auto ObjQueue::operator=(ObjQueue&& other) -> ObjQueue&
    {
        swap(*this, other);
        // Leaving other empty to make sure resources are no longer used
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
        queue_push(raw(), id);
    }

    void ObjQueue::push_back(value_type id1, value_type id2)
    {
        queue_push2(raw(), id1, id2);
    }

    auto ObjQueue::insert(const_iterator pos, value_type id) -> iterator
    {
        const auto offset = offset_of(pos);
        insert(offset, id);
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
        const auto offset = offset_of(pos);
        assert(offset <= std::numeric_limits<int>::max());
        queue_delete(raw(), static_cast<int>(offset));
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
        queue_prealloc(raw(), static_cast<int>(cap_diff));
    }

    void ObjQueue::clear()
    {
        queue_empty(raw());
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

    auto ObjQueue::operator[](size_type pos) -> reference
    {
        return data()[pos];
    }

    auto ObjQueue::operator[](size_type pos) const -> const_reference
    {
        return data()[pos];
    }

    namespace
    {
        void ensure_valid_pos(ObjQueue::size_type size, ObjQueue::size_type pos)
        {
            if (pos >= size)
            {
                // TODO(C++20) std::format
                auto ss = std::stringstream{};
                ss << "Index " << pos << " is greater that the number of elements (" << size << ')';
                throw std::out_of_range(std::move(ss).str());
            }
        }
    }

    auto ObjQueue::at(size_type pos) -> reference
    {
        ensure_valid_pos(size(), pos);
        return operator[](pos);
    }

    auto ObjQueue::at(size_type pos) const -> const_reference
    {
        ensure_valid_pos(size(), pos);
        return operator[](pos);
    }

    auto ObjQueue::begin() -> iterator
    {
        return data();
    }

    auto ObjQueue::begin() const -> const_iterator
    {
        return cbegin();
    }

    auto ObjQueue::cbegin() const -> const_iterator
    {
        return data();
    }

    auto ObjQueue::end() -> iterator
    {
        return data() + size();
    }

    auto ObjQueue::end() const -> const_iterator
    {
        return cend();
    }

    auto ObjQueue::cend() const -> const_iterator
    {
        return data() + size();
    }

    auto ObjQueue::rbegin() -> reverse_iterator
    {
        return std::reverse_iterator{ end() };
    }

    auto ObjQueue::rbegin() const -> const_reverse_iterator
    {
        return crbegin();
    }

    auto ObjQueue::crbegin() const -> const_reverse_iterator
    {
        return std::reverse_iterator{ end() };
    }

    auto ObjQueue::rend() -> reverse_iterator
    {
        return std::reverse_iterator{ begin() };
    }

    auto ObjQueue::rend() const -> const_reverse_iterator
    {
        return crend();
    }

    auto ObjQueue::crend() const -> const_reverse_iterator
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

    auto ObjQueue::contains(value_type id) const -> bool
    {
        return std::find(cbegin(), cend(), id) != cend();
    }

    auto ObjQueue::raw() const -> const ::Queue*
    {
        return &m_queue;
    }

    auto ObjQueue::raw() -> ::Queue*
    {
        return &m_queue;
    }

    auto ObjQueue::offset_of(const_iterator pos) const -> size_type
    {
        const auto offset = std::distance(begin(), pos);
        assert(offset >= 0);
        return static_cast<std::size_t>(offset);
    }

    void ObjQueue::insert(size_type offset, value_type id)
    {
        assert(offset <= std::numeric_limits<int>::max());
        queue_insert(raw(), static_cast<int>(offset), id);
    }

    void ObjQueue::insert_n(size_type offset, const_iterator first, size_type n)
    {
        assert(offset <= std::numeric_limits<int>::max());
        assert(n <= std::numeric_limits<int>::max());
        queue_insertn(raw(), static_cast<int>(offset), static_cast<int>(n), first);
    }

    void swap(ObjQueue& a, ObjQueue& b) noexcept
    {
        using std::swap;
        swap(a.m_queue, b.m_queue);
    }

    auto operator==(const ObjQueue& a, const ObjQueue& b) -> bool
    {
        return std::equal(a.cbegin(), a.cend(), b.cbegin(), b.cend());
    }

    auto operator!=(const ObjQueue& a, const ObjQueue& b) -> bool
    {
        return !(a == b);
    }
}
