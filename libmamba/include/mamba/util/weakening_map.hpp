// Copyright (c) 20123 QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_WEAKENING_MAP_HPP
#define MAMBA_UTIL_WEAKENING_MAP_HPP

#include <stdexcept>

#include <fmt/format.h>

namespace mamba::util
{
    /**
     * A Map wrapper that can weaken a key to find more matches.
     *
     * The API of a standard map is unmodified, only methods ending with "weaken" look for
     * multiple keys.
     * This can be understood as an extreme generalization of defaults: when a key is not found,
     * the behaviour is to look for another key.
     * The behaviour for generating the sequence of weaken keys is controlled by the Weakener.
     */
    template <typename Map, typename Weakener>
    class weakening_map : private Map
    {
    public:

        using Base = Map;
        using typename Base::key_type;
        using typename Base::mapped_type;
        using typename Base::value_type;
        using typename Base::size_type;
        using typename Base::iterator;
        using typename Base::const_iterator;
        using weakener_type = Weakener;

        using Base::Base;
        explicit weakening_map(Base map);
        template <typename... Args>
        explicit weakening_map(weakener_type weakener, Args&&... args);

        [[nodiscard]] auto generic() const -> const Base&;

        using Base::begin;
        using Base::end;
        using Base::cbegin;
        using Base::cend;

        using Base::empty;
        using Base::max_size;

        using Base::clear;
        using Base::insert;
        using Base::insert_or_assign;
        using Base::emplace;
        using Base::emplace_hint;
        using Base::try_emplace;
        using Base::erase;
        using Base::swap;
        using Base::extract;
        using Base::merge;

        using Base::reserve;

        using Base::at;

        [[nodiscard]] auto size() const -> std::size_t;

        [[nodiscard]] auto at_weaken(const key_type& key) -> mapped_type&;
        [[nodiscard]] auto at_weaken(const key_type& key) const -> const mapped_type&;

        using Base::find;

        auto find_weaken(const key_type& key) -> iterator;
        auto find_weaken(const key_type& key) const -> const_iterator;

        [[nodiscard]] auto contains(const key_type& key) const -> bool;

        [[nodiscard]] auto contains_weaken(const key_type& key) const -> bool;

    private:

        Weakener m_weakener = {};
    };

    template <typename M, typename W>
    auto operator==(const weakening_map<M, W>& a, const weakening_map<M, W>& b) -> bool;
    template <typename M, typename W>
    auto operator!=(const weakening_map<M, W>& a, const weakening_map<M, W>& b) -> bool;

    /*************************************
     *  Implementation of weakening_map  *
     *************************************/

    template <typename M, typename W>
    weakening_map<M, W>::weakening_map(Base map)
        : Base(std::move(map))
    {
    }

    template <typename M, typename W>
    template <typename... Args>
    weakening_map<M, W>::weakening_map(weakener_type weakener, Args&&... args)
        : Base(std::forward<Args>(args)...)
        , m_weakener(std::move(weakener))
    {
    }

    template <typename M, typename W>
    auto weakening_map<M, W>::generic() const -> const Base&
    {
        return *this;
    }

    template <typename M, typename W>
    auto weakening_map<M, W>::size() const -> std::size_t
    {
        // https://github.com/pybind/pybind11/pull/4952
        return Base::size();
    }

    template <typename M, typename W>
    auto weakening_map<M, W>::at_weaken(const key_type& key) -> mapped_type&
    {
        if (auto it = find_weaken(key); it != end())
        {
            return it->second;
        }
        throw std::out_of_range(fmt::format(R"(No entry for key "{}")", key));
    }

    template <typename M, typename W>
    auto weakening_map<M, W>::at_weaken(const key_type& key) const -> const mapped_type&
    {
        return const_cast<weakening_map<M, W>*>(this)->at_weaken(key);
    }

    template <typename M, typename W>
    auto weakening_map<M, W>::find_weaken(const key_type& key) -> iterator
    {
        auto k = key_type(m_weakener.make_first_key(key));
        auto it = Base::find(k);
        while (it == Base::end())
        {
            if (auto kk = m_weakener.weaken_key(k))
            {
                // Try weakening the key more
                k = *kk;
                it = Base::find(k);
            }
            else
            {
                // No more weakened key to try
                return Base::end();
            }
        }
        return it;
    }

    template <typename M, typename W>
    auto weakening_map<M, W>::find_weaken(const key_type& key) const -> const_iterator
    {
        return static_cast<const_iterator>(const_cast<weakening_map<M, W>*>(this)->find_weaken(key));
    }

    template <typename M, typename W>
    auto weakening_map<M, W>::contains(const key_type& key) const -> bool
    {
        return find(key) != end();
    }

    template <typename M, typename W>
    auto weakening_map<M, W>::contains_weaken(const key_type& key) const -> bool
    {
        return find_weaken(key) != end();
    }

    template <typename M, typename W>
    auto operator==(const weakening_map<M, W>& a, const weakening_map<M, W>& b) -> bool
    {
        return a.generic() == b.generic();
    }

    template <typename M, typename W>
    auto operator!=(const weakening_map<M, W>& a, const weakening_map<M, W>& b) -> bool
    {
        return a.generic() != b.generic();
    }
}
#endif
