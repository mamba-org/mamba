// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#ifndef MAMBA_UTILFLAT_SET_HPP
#define MAMBA_UTILFLAT_SET_HPP

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "mamba/util/deprecation.hpp"
#include "mamba/util/tuple_hash.hpp"

namespace mamba::util
{

    struct sorted_unique_t
    {
        explicit sorted_unique_t() = default;
    };

    inline constexpr sorted_unique_t sorted_unique{};

    /**
     * A sorted vector behaving like a set.
     *
     * Like, ``std::set``, uniqueness is determined by using the equivalence relation.
     * In imprecise terms, two objects ``a`` and ``b`` are considered equivalent if neither
     * compares less than the other: ``!comp(a, b) && !comp(b, a)``
     */
    template <typename Key, typename Compare = std::less<Key>, typename Allocator = std::allocator<Key>>
    class MAMBA_DEPRECATED_CXX23 flat_set : private std::vector<Key, Allocator>
    {
    public:

        using Base = std::vector<Key, Allocator>;
        using typename Base::allocator_type;
        using typename Base::const_iterator;
        using typename Base::const_reverse_iterator;
        using typename Base::size_type;
        using typename Base::value_type;
        using key_compare = Compare;
        using value_compare = Compare;

        using Base::cbegin;
        using Base::cend;
        using Base::crbegin;
        using Base::crend;

        using Base::clear;
        using Base::empty;
        using Base::reserve;
        using Base::size;

        flat_set() = default;
        flat_set(
            std::initializer_list<value_type> il,
            key_compare compare = key_compare(),
            const allocator_type& alloc = allocator_type()
        );
        template <typename InputIterator>
        flat_set(
            InputIterator first,
            InputIterator last,
            key_compare compare = key_compare(),
            const allocator_type& alloc = Allocator()
        );
        template <typename InputIterator>
        flat_set(
            sorted_unique_t,
            InputIterator first,
            InputIterator last,
            key_compare compare = key_compare(),
            const allocator_type& alloc = Allocator()
        );
        flat_set(const flat_set&) = default;
        flat_set(flat_set&&) = default;
        explicit flat_set(std::vector<Key, Allocator>&& other, key_compare compare = key_compare());
        explicit flat_set(const std::vector<Key, Allocator>& other, key_compare compare = key_compare());

        auto operator=(const flat_set&) -> flat_set& = default;
        auto operator=(flat_set&&) -> flat_set& = default;

        auto key_comp() const -> const key_compare&;

        auto front() const noexcept -> const value_type&;
        auto back() const noexcept -> const value_type&;
        auto operator[](size_type pos) const -> const value_type&;
        auto at(size_type pos) const -> const value_type&;

        auto begin() const noexcept -> const_iterator;
        auto end() const noexcept -> const_iterator;
        auto rbegin() const noexcept -> const_reverse_iterator;
        auto rend() const noexcept -> const_reverse_iterator;

        /** Insert an element in the set.
         *
         * Like std::vector and unlike std::set, inserting an element invalidates iterators.
         */
        auto insert(value_type&& value) -> std::pair<const_iterator, bool>;
        auto insert(const value_type& value) -> std::pair<const_iterator, bool>;
        template <typename InputIterator>
        void insert(InputIterator first, InputIterator last);

        auto erase(const_iterator pos) -> const_iterator;
        auto erase(const_iterator first, const_iterator last) -> const_iterator;
        auto erase(const value_type& value) -> size_type;

        template <class T>
        auto contains(const T& value) const -> bool;

    private:

        key_compare m_compare;

        auto key_eq(const value_type& a, const value_type& b) const -> bool;
        template <typename U>
        auto insert_impl(U&& value) -> std::pair<const_iterator, bool>;
        void sort_and_remove_duplicates();

        template <typename K, typename C, typename A>
        friend auto operator==(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> bool;

        template <typename K, typename C, typename A>
        friend auto set_union(const flat_set<K, C, A>&, const flat_set<K, C, A>&)
            -> flat_set<K, C, A>;
        template <typename K, typename C, typename A>
        friend auto set_intersection(const flat_set<K, C, A>&, const flat_set<K, C, A>&)
            -> flat_set<K, C, A>;
        template <typename K, typename C, typename A>
        friend auto set_difference(const flat_set<K, C, A>&, const flat_set<K, C, A>&)
            -> flat_set<K, C, A>;
        template <typename K, typename C, typename A>
        friend auto set_symmetric_difference(const flat_set<K, C, A>&, const flat_set<K, C, A>&)
            -> flat_set<K, C, A>;
    };

    template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
    flat_set(std::initializer_list<Key>, Compare = Compare(), Allocator = Allocator())
        -> flat_set<Key, Compare, Allocator>;

    template <
        class InputIt,
        class Comp = std::less<typename std::iterator_traits<InputIt>::value_type>,
        class Alloc = std::allocator<typename std::iterator_traits<InputIt>::value_type>>
    flat_set(InputIt, InputIt, Comp = Comp(), Alloc = Alloc())
        -> flat_set<typename std::iterator_traits<InputIt>::value_type, Comp, Alloc>;

    template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
    flat_set(std::vector<Key, Allocator>&&, Compare compare = Compare())
        -> flat_set<Key, Compare, Allocator>;

    template <class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>>
    flat_set(const std::vector<Key, Allocator>&, Compare compare = Compare())
        -> flat_set<Key, Compare, Allocator>;

    template <typename Key, typename Compare, typename Allocator>
    auto
    operator==(const flat_set<Key, Compare, Allocator>& lhs, const flat_set<Key, Compare, Allocator>& rhs)
        -> bool;

    template <typename Key, typename Compare, typename Allocator>
    auto
    operator!=(const flat_set<Key, Compare, Allocator>& lhs, const flat_set<Key, Compare, Allocator>& rhs)
        -> bool;

    template <typename Key, typename Compare, typename Allocator>
    auto set_is_disjoint_of(
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> bool;

    template <typename Key, typename Compare, typename Allocator>
    auto is_subset_of(
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> bool;

    template <typename Key, typename Compare, typename Allocator>
    auto is_strict_subset_of(
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> bool;

    template <typename Key, typename Compare, typename Allocator>
    auto is_superset_of(
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> bool;

    template <typename Key, typename Compare, typename Allocator>
    auto is_strict_superset_of(
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> bool;

    template <typename Key, typename Compare, typename Allocator>
    auto set_union(  //
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> flat_set<Key, Compare, Allocator>;

    template <typename Key, typename Compare, typename Allocator>
    auto set_intersection(
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> flat_set<Key, Compare, Allocator>;

    template <typename Key, typename Compare, typename Allocator>
    auto set_difference(
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> flat_set<Key, Compare, Allocator>;

    template <typename Key, typename Compare, typename Allocator>
    auto set_symmetric_difference(
        const flat_set<Key, Compare, Allocator>& lhs,
        const flat_set<Key, Compare, Allocator>& rhs
    ) -> flat_set<Key, Compare, Allocator>;

    /*******************************
     *  vector_set Implementation  *
     *******************************/

    template <typename K, typename C, typename A>
    flat_set<K, C, A>::flat_set(
        std::initializer_list<value_type> il,
        key_compare compare,
        const allocator_type& alloc
    )
        : Base(std::move(il), alloc)
        , m_compare(std::move(compare))
    {
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    template <typename InputIterator>
    flat_set<K, C, A>::flat_set(
        InputIterator first,
        InputIterator last,
        key_compare compare,
        const allocator_type& alloc
    )
        : Base(first, last, alloc)
        , m_compare(std::move(compare))
    {
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    template <typename InputIterator>
    flat_set<K, C, A>::flat_set(
        sorted_unique_t,
        InputIterator first,
        InputIterator last,
        key_compare compare,
        const allocator_type& alloc
    )
        : Base(first, last, alloc)
        , m_compare(std::move(compare))
    {
    }

    template <typename K, typename C, typename A>
    flat_set<K, C, A>::flat_set(std::vector<K, A>&& other, C compare)
        : Base(std::move(other))
        , m_compare(std::move(compare))
    {
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    flat_set<K, C, A>::flat_set(const std::vector<K, A>& other, C compare)
        : Base(std::move(other))
        , m_compare(std::move(compare))
    {
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::key_comp() const -> const key_compare&
    {
        return m_compare;
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::front() const noexcept -> const value_type&
    {
        return Base::front();
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::back() const noexcept -> const value_type&
    {
        return Base::back();
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::operator[](size_type pos) const -> const value_type&
    {
        return Base::operator[](pos);
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::at(size_type pos) const -> const value_type&
    {
        return Base::at(pos);
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::begin() const noexcept -> const_iterator
    {
        return Base::begin();
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::end() const noexcept -> const_iterator
    {
        return Base::end();
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::rbegin() const noexcept -> const_reverse_iterator
    {
        return Base::rbegin();
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::rend() const noexcept -> const_reverse_iterator
    {
        return Base::rend();
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::insert(const value_type& value) -> std::pair<const_iterator, bool>
    {
        return insert_impl(value);
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::insert(value_type&& value) -> std::pair<const_iterator, bool>
    {
        return insert_impl(std::move(value));
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::key_eq(const value_type& a, const value_type& b) const -> bool
    {
        return !m_compare(a, b) && !m_compare(b, a);
    }

    template <typename K, typename C, typename A>
    void flat_set<K, C, A>::sort_and_remove_duplicates()
    {
        std::sort(Base::begin(), Base::end(), m_compare);
        auto is_eq = [this](const value_type& a, const value_type& b) { return key_eq(a, b); };
        Base::erase(std::unique(Base::begin(), Base::end(), is_eq), Base::end());
    }

    template <typename K, typename C, typename A>
    template <typename InputIterator>
    void flat_set<K, C, A>::insert(InputIterator first, InputIterator last)
    {
        Base::insert(Base::end(), first, last);
        sort_and_remove_duplicates();
    }

    template <typename K, typename C, typename A>
    template <typename U>
    auto flat_set<K, C, A>::insert_impl(U&& value) -> std::pair<const_iterator, bool>
    {
        auto it = std::lower_bound(begin(), end(), value, m_compare);
        if ((it != end()) && (key_eq(*it, value)))
        {
            return { it, false };
        }
        it = Base::insert(it, std::forward<U>(value));
        return { it, true };
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::erase(const_iterator pos) -> const_iterator
    {
        // No need to sort or remove duplicates again
        return Base::erase(pos);
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::erase(const_iterator first, const_iterator last) -> const_iterator
    {
        // No need to sort or remove duplicates again
        return Base::erase(first, last);
    }

    template <typename K, typename C, typename A>
    auto flat_set<K, C, A>::erase(const value_type& value) -> size_type
    {
        auto it = std::lower_bound(begin(), end(), value, m_compare);
        if ((it == end()) || (!(key_eq(*it, value))))
        {
            return 0;
        }
        erase(it);
        return 1;
    }

    template <typename K, typename C, typename A>
    template <class T>
    auto flat_set<K, C, A>::contains(const T& value) const -> bool
    {
        return std::binary_search(begin(), end(), value);
    }

    namespace detail
    {
        /**
         * Check if two sorted range have an empty intersection.
         *
         * Edited from https://en.cppreference.com/w/cpp/algorithm/set_intersection
         * Distributed under the terms of the Copyright/CC-BY-SA License.
         * The full license can be found at the address
         * https://en.cppreference.com/w/Cppreference:Copyright/CC-BY-SA
         */
        template <class InputIt1, class InputIt2, class Compare>
        auto
        set_disjoint(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, Compare comp)
            -> bool
        {
            while (first1 != last1 && first2 != last2)
            {
                if (comp(*first1, *first2))
                {
                    ++first1;
                }
                else
                {
                    if (!comp(*first2, *first1))
                    {
                        return false;  // *first1 and *first2 are equivalent.
                    }
                    ++first2;
                }
            }
            return true;
        }
    }

    template <typename K, typename C, typename A>
    auto operator==(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> bool
    {
        auto is_eq = [&lhs](const auto& a, const auto& b) { return lhs.key_eq(a, b); };
        return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), is_eq);
    }

    template <typename K, typename C, typename A>
    auto operator!=(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> bool
    {
        return !(lhs == rhs);
    }

    template <typename K, typename C, typename A>
    auto set_is_disjoint_of(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> bool
    {
        return detail::set_disjoint(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), lhs.key_comp());
    }

    template <typename K, typename C, typename A>
    auto set_is_subset_of(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> bool
    {
        return (lhs.size() <= rhs.size())  // For perf
               && std::includes(rhs.cbegin(), rhs.cend(), lhs.cbegin(), lhs.cend(), lhs.key_comp());
    }

    template <typename K, typename C, typename A>
    auto set_is_strict_subset_of(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> bool
    {
        return (lhs.size() < rhs.size()) && set_is_subset_of(lhs, rhs);
    }

    template <typename K, typename C, typename A>
    auto set_is_superset_of(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> bool
    {
        return set_is_subset_of(rhs, lhs);
    }

    template <typename K, typename C, typename A>
    auto set_is_strict_superset_of(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> bool
    {
        return set_is_strict_subset_of(rhs, lhs);
    }

    template <typename K, typename C, typename A>
    auto set_union(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs) -> flat_set<K, C, A>
    {
        auto out = flat_set<K, C, A>();
        out.reserve(std::max(lhs.size(), rhs.size()));  // lower bound
        std::set_union(
            lhs.cbegin(),
            lhs.cend(),
            rhs.cbegin(),
            rhs.cend(),
            std::back_inserter(static_cast<typename flat_set<K, C, A>::Base&>(out)),
            lhs.m_compare
        );
        return out;
    }

    template <typename K, typename C, typename A>
    auto set_intersection(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs)
        -> flat_set<K, C, A>
    {
        auto out = flat_set<K, C, A>();
        std::set_intersection(
            lhs.cbegin(),
            lhs.cend(),
            rhs.cbegin(),
            rhs.cend(),
            std::back_inserter(static_cast<typename flat_set<K, C, A>::Base&>(out)),
            lhs.m_compare
        );
        return out;
    }

    template <typename K, typename C, typename A>
    auto set_difference(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs)
        -> flat_set<K, C, A>
    {
        auto out = flat_set<K, C, A>();
        std::set_difference(
            lhs.cbegin(),
            lhs.cend(),
            rhs.cbegin(),
            rhs.cend(),
            std::back_inserter(static_cast<typename flat_set<K, C, A>::Base&>(out)),
            lhs.m_compare
        );
        return out;
    }

    template <typename K, typename C, typename A>
    auto set_symmetric_difference(const flat_set<K, C, A>& lhs, const flat_set<K, C, A>& rhs)
        -> flat_set<K, C, A>
    {
        auto out = flat_set<K, C, A>();
        std::set_symmetric_difference(
            lhs.cbegin(),
            lhs.cend(),
            rhs.cbegin(),
            rhs.cend(),
            std::back_inserter(static_cast<typename flat_set<K, C, A>::Base&>(out)),
            lhs.m_compare
        );
        return out;
    }
}

template <typename Key, typename Compare, typename Allocator>
struct std::hash<mamba::util::flat_set<Key, Compare, Allocator>>
{
    auto operator()(const mamba::util::flat_set<Key, Compare, Allocator>& set) const -> std::size_t
    {
        return mamba::util::hash_range(set);
    }
};

#endif
