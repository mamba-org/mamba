// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_TUPLE_HASH_HPP
#define MAMBA_UTIL_TUPLE_HASH_HPP

#include <functional>
#include <tuple>
#include <type_traits>

namespace mamba::util
{
    constexpr auto hash_combine(std::size_t seed, std::size_t other) -> std::size_t
    {
        const auto boost_magic_num = 0x9e3779b9;
        seed ^= other + boost_magic_num + (seed << 6) + (seed >> 2);
        return seed;
    }

    template <class T, typename Hasher = std::hash<T>>
    constexpr auto hash_combine_val(std::size_t seed, const T& val, const Hasher& hasher = {})
        -> std::size_t
    {
        return hash_combine(seed, hasher(val));
    }

    template <typename Iter, typename Hasher = std::hash<std::decay_t<decltype(*std::declval<Iter>())>>>
    auto hash_combine_val_range(std::size_t seed, Iter first, Iter last, const Hasher& hasher = {})
        -> std::size_t
    {
        for (; first != last; ++first)
        {
            seed = hash_combine_val(seed, hasher(*first));
        }
        return seed;
    }

    template <typename... T>
    constexpr auto hash_vals(const T&... vals) -> std::size_t
    {
        std::size_t seed = 0;
        auto combine = [&seed](const auto& val) { seed = hash_combine_val(seed, val); };
        (combine(vals), ...);
        return seed;
    }

    template <typename... T>
    constexpr auto hash_tuple(const std::tuple<T...>& t) -> std::size_t
    {
        return std::apply([](const auto&... vals) { return hash_vals(vals...); }, t);
    }

    template <typename... T>
    struct Tuplehasher
    {
        constexpr auto operator()(const std::tuple<T...>& t) const -> std::size_t
        {
            return hash_tuple(t);
        }
    };

    template <typename Range>
    constexpr auto hash_range(const Range& rng) -> std::size_t
    {
        return hash_combine_val_range(0, rng.begin(), rng.end());
    }
}
#endif
