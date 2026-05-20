// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

//////////////////////////////////////////////////////////////////////////////
// This file provides implementations to generic algorithms which are not yet
// available in our current C++ version and/or implementations.
//
// TODO: replace these implementations by standard implementations
//       once available.
//

#ifndef MAMBA_UTIL_ALGORITHM_HPP
#define MAMBA_UTIL_ALGORITHM_HPP

#include <algorithm>
#include <concepts>
#include <iterator>
#include <ranges>

namespace mamba::stdext  // not using mamba::util because of potential conflicts with existing
                         // tooling
{
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // see https://en.cppreference.com/w/cpp/algorithm/ranges/contains.html
    namespace details
    {
        struct contains_fn
        {
            template <std::input_iterator I, std::sentinel_for<I> S, class Proj = std::identity, class T>
                requires std::indirect_binary_predicate<std::ranges::equal_to, std::projected<I, Proj>, const T*>
            constexpr bool operator()(I first, S last, const T& value, Proj proj = {}) const
            {
                return std::ranges::find(std::move(first), last, value, proj) != last;
            }

            template <std::ranges::input_range R, class Proj = std::identity, class T>
                requires std::indirect_binary_predicate<
                    std::ranges::equal_to,
                    std::projected<std::ranges::iterator_t<R>, Proj>,
                    const T*>
            constexpr bool operator()(R&& r, const T& value, Proj proj = {}) const
            {
                return std::ranges::find(std::ranges::begin(r), std::ranges::end(r), value, proj)
                       != std::ranges::end(r);
            }
        };
    }

    inline constexpr details::contains_fn contains{};

    /////////////////////////////////////////////////////////////////////////////////////////////////

}
#endif
