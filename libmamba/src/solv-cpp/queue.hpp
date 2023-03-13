// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_SOLV_QUEUE_HPP
#define MAMBA_SOLV_QUEUE_HPP

#include <iterator>
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
        ObjQueue(ObjQueue&& other);
        ObjQueue(const ObjQueue& other);

        ~ObjQueue();

        auto operator=(ObjQueue&& other) -> ObjQueue&;
        auto operator=(const ObjQueue& other) -> ObjQueue&;

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
        auto operator[](size_type pos) -> reference;
        auto operator[](size_type pos) const -> const_reference;
        auto at(size_type pos) -> reference;
        auto at(size_type pos) const -> const_reference;
        auto begin() -> iterator;
        auto begin() const -> const_iterator;
        auto cbegin() const -> const_iterator;
        auto end() -> iterator;
        auto end() const -> const_iterator;
        auto cend() const -> const_iterator;
        auto rbegin() -> reverse_iterator;
        auto rbegin() const -> const_reverse_iterator;
        auto crbegin() const -> const_reverse_iterator;
        auto rend() -> reverse_iterator;
        auto rend() const -> const_reverse_iterator;
        auto crend() const -> const_reverse_iterator;
        auto data() -> pointer;
        auto data() const -> const_pointer;

        template <template <typename, typename...> class C>
        auto as() -> C<value_type>;

        auto raw() -> ::Queue*;
        auto raw() const -> const ::Queue*;

    private:

        friend void swap(ObjQueue& a, ObjQueue& b) noexcept;

        ::Queue m_queue = {};

        explicit ObjQueue(std::nullptr_t);

        auto offset_of(const_iterator pos) const -> size_type;
        void insert(size_type offset, value_type id);
        void insert_n(size_type offset, const_iterator first, size_type n);
    };

    void swap(ObjQueue& a, ObjQueue& b) noexcept;

    /********************************
     *  Implementation of ObjQueue  *
     ********************************/

    template <typename InputIt>
    ObjQueue::ObjQueue(InputIt first, InputIt last)
        : ObjQueue()
    {
        insert(begin(), first, last);
    }

    template <typename InputIt>
    auto ObjQueue::insert(const_iterator pos, InputIt first, InputIt last) -> iterator
    {
        const auto offset = offset_of(pos);
        const auto n = std::distance(first, last);

        if constexpr (std::is_same_v<InputIt, iterator> || std::is_same_v<InputIt, const_iterator>)
        {
            insert_n(offset, first, static_cast<size_type>(n));
        }
        else
        {
            reserve(size() + static_cast<size_type>(n));  // invalidates `pos` iterator
            for (auto o = offset; first != last; ++first, ++o)
            {
                insert(o, *first);
            }
        }
        return begin() + offset;
    }

    template <template <typename, typename...> class C>
    auto ObjQueue::as() -> C<value_type>
    {
        return C<value_type>(begin(), end());
    }

}  // namespace mamba

#endif
