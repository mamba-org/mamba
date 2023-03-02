// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_QUEUE_HPP
#define MAMBA_SOLV_QUEUE_HPP

#include <cassert>
#include <iterator>
#include <limits>
#include <utility>

#include <solv/queue.h>

namespace mamba::solv
{
    /**
     * A ``std::vector`` like structure used in libsolv.
     *
     * This is used privately within libsolv, hence the direct use of ``Queue`` as an attribute
     * (for performances).
     */
    class ObjQueue
    {
    public:

        using value_type = ::Id;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = ::Id*;
        using const_pointer = const ::Id*;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using iterator = pointer;
        using const_iterator = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        ObjQueue();
        template <typename InputIt>
        ObjQueue(InputIt first, InputIt last);
        ObjQueue(std::initializer_list<value_type> elems);
        ~ObjQueue();

        /**
         * Move and copy operations are disabled for simplicity but could be added if needed.
         */
        ObjQueue(const ObjQueue&) = delete;
        ObjQueue(ObjQueue&&) = delete;
        ObjQueue& operator=(const ObjQueue&) = delete;
        ObjQueue& operator=(ObjQueue&&) = delete;

        auto size() const -> size_type;
        auto capacity() const -> size_type;
        auto empty() const -> bool;

        auto insert(const_iterator pos, value_type id) -> iterator;
        template <typename InputIt>
        auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator;
        void push_back(value_type id);
        void push_back(value_type id1, value_type id2);
        auto erase(const_iterator pos) -> iterator;
        void reserve(size_type new_cap);
        void clear();

        auto front() -> reference;
        auto front() const -> const_reference;
        auto back() -> reference;
        auto back() const -> const_reference;
        auto operator[](int idx) -> reference;
        auto operator[](int idx) const -> const_reference;
        auto begin() -> iterator;
        auto begin() const -> const_iterator;
        auto end() -> iterator;
        auto end() const -> const_iterator;
        auto rbegin() -> reverse_iterator;
        auto rbegin() const -> const_reverse_iterator;
        auto rend() -> reverse_iterator;
        auto rend() const -> const_reverse_iterator;

        template <template <typename, typename...> class C>
        auto as() -> C<value_type>;

        auto get() -> ::Queue*;
        auto get() const -> const ::Queue*;

    private:

        ::Queue m_queue = {};
    };

    /********************************
     *  Implementation of ObjQueue  *
     ********************************/

    inline ObjQueue::ObjQueue()
    {
        queue_init(get());
    }

    template <typename InputIt>
    inline ObjQueue::ObjQueue(InputIt first, InputIt last)
        : ObjQueue()
    {
        insert(begin(), first, last);
    }

    inline ObjQueue::ObjQueue(std::initializer_list<value_type> elems)
        : ObjQueue(elems.begin(), elems.end())
    {
    }

    inline ObjQueue::~ObjQueue()
    {
        queue_free(get());
    }

    inline void ObjQueue::push_back(value_type id)
    {
        queue_push(get(), id);
    }

    inline void ObjQueue::push_back(value_type id1, value_type id2)
    {
        queue_push2(get(), id1, id2);
    }

    inline auto ObjQueue::insert(const_iterator pos, value_type id) -> iterator
    {
        const difference_type offset = pos - begin();
        assert(offset >= 0);
        assert(offset <= std::numeric_limits<int>::max());
        queue_insert(get(), static_cast<int>(offset), id);
        return begin() + offset;
    }

    template <typename InputIt>
    auto ObjQueue::insert(const_iterator pos, InputIt first, InputIt last) -> iterator
    {
        const difference_type offset = pos - begin();
        assert(offset >= 0);
        assert(offset <= std::numeric_limits<int>::max());
        const auto n = std::distance(first, last);
        assert(n >= 0);

        if constexpr (std::is_same_v<InputIt, iterator> || std::is_same_v<InputIt, const_iterator>)
        {
            assert(n <= std::numeric_limits<int>::max());
            queue_insertn(get(), static_cast<int>(offset), static_cast<int>(n), first);
        }
        else
        {
            reserve(size() + n);
            for (; first != last; ++first)
            {
                pos = insert(pos, *first);
                ++pos;
            }
        }
        return begin() + offset;
    }

    inline auto ObjQueue::capacity() const -> size_type
    {
        return static_cast<size_type>(m_queue.count + m_queue.left);
    }

    inline auto ObjQueue::size() const -> size_type
    {
        assert(m_queue.count >= 0);
        return static_cast<std::size_t>(m_queue.count);
    }

    inline auto ObjQueue::empty() const -> bool
    {
        return size() == 0;
    }

    inline auto ObjQueue::erase(const_iterator pos) -> iterator
    {
        const difference_type offset = pos - begin();
        assert(offset >= 0);
        assert(offset <= std::numeric_limits<int>::max());
        queue_delete(get(), static_cast<int>(offset));
        return begin() + offset;
    }

    inline void ObjQueue::reserve(size_type new_cap)
    {
        if (new_cap < capacity())
        {
            return;
        }
        size_type cap_diff = new_cap - capacity();
        assert(cap_diff <= std::numeric_limits<int>::max());
        queue_prealloc(get(), static_cast<int>(cap_diff));
    }

    inline void ObjQueue::clear()
    {
        queue_empty(get());
    }

    inline auto ObjQueue::front() -> reference
    {
        return *begin();
    }

    inline auto ObjQueue::front() const -> const_reference
    {
        return *begin();
    }

    inline auto ObjQueue::back() -> reference
    {
        return *rbegin();
    }

    inline auto ObjQueue::back() const -> const_reference
    {
        return *rbegin();
    }

    inline auto ObjQueue::operator[](int idx) -> reference
    {
        return m_queue.elements[idx];
    }

    inline auto ObjQueue::operator[](int idx) const -> const_reference
    {
        return m_queue.elements[idx];
    }

    inline auto ObjQueue::begin() -> iterator
    {
        return m_queue.elements;
    }

    inline auto ObjQueue::begin() const -> const_iterator
    {
        return m_queue.elements;
    }

    inline auto ObjQueue::end() -> iterator
    {
        return m_queue.elements + size();
    }

    inline auto ObjQueue::end() const -> const_iterator
    {
        return m_queue.elements + size();
    }

    inline auto ObjQueue::rbegin() -> reverse_iterator
    {
        return std::reverse_iterator{ end() };
    }

    inline auto ObjQueue::rbegin() const -> const_reverse_iterator
    {
        return std::reverse_iterator{ end() };
    }

    inline auto ObjQueue::rend() -> reverse_iterator
    {
        return std::reverse_iterator{ begin() };
    }

    inline auto ObjQueue::rend() const -> const_reverse_iterator
    {
        return std::reverse_iterator{ begin() };
    }

    template <template <typename, typename...> class C>
    auto ObjQueue::as() -> C<value_type>
    {
        return C<value_type>(begin(), end());
    }

    inline const ::Queue* ObjQueue::get() const
    {
        return &m_queue;
    }

    inline ::Queue* ObjQueue::get()
    {
        return &m_queue;
    }

}  // namespace mamba

#endif
