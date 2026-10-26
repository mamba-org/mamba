// Copyright (c) 2025, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

//////////////////////////////////////////////////////////////////////////////
// This file provides implementations og ranges features which are not yet
// available in our current C++ version and/or implementations.
//
// TODO: replace these implementations by standard implementations
//       once available.
//

#ifndef MAMBA_UTIL_RANGES_HPP
#define MAMBA_UTIL_RANGES_HPP

#include <ranges>

namespace mamba::rangesext
{
    namespace detail
    {
        template <typename T>
        inline constexpr bool has_reserve_v = requires(T& t, std::size_t n) { t.reserve(n); };

        template <typename Container, std::ranges::input_range Range>
        inline constexpr bool toable = requires {
            requires(
                !std::ranges::input_range<Container>
                || std::convertible_to<std::ranges::range_reference_t<Range>, std::ranges::range_value_t<Container>>
            );
        };
    }

    // Back port of std::ranges::to
    template <class Container, std::ranges::range R, class... Args>
    auto to(R&& r, Args&&... args) -> Container
    {
        if constexpr (detail::toable<Container, R>)
        {
            if constexpr (std::ranges::common_range<R>)
            {
                return Container(std::ranges::begin(r), std::ranges::end(r), std::forward<Args>(args)...);
            }
            else
            {
                static_assert(std::constructible_from<Container, Args...>);
                Container c(std::forward<Args>(args)...);
                if constexpr (std::ranges::sized_range<R> && detail::has_reserve_v<Container>)
                {
                    c.reserve(std::ranges::size(r));
                }
                auto it = std::ranges::begin(r);
                auto sent = std::ranges::end(r);
                while (it != sent)
                {
                    if constexpr (requires { c.emplace_back(*it); })
                    {
                        c.emplace_back(*it);
                    }
                    else if constexpr (requires { c.push_back(*it); })
                    {
                        c.push_back(*it);
                    }
                    else if constexpr (requires { c.emplace(c.end(), *it); })
                    {
                        c.emplace(c.end(), *it);
                    }
                    else
                    {
                        c.insert(c.end(), *it);
                    }
                    ++it;
                }
                return c;
            }
        }
        else
        {
            static_assert(std::ranges::input_range<std::ranges::range_reference_t<R>>);
            return to<Container>(
                std::ranges::ref_view(r)
                    | std::views::transform(
                        []<typename element_type>(element_type&& elem)
                        {
                            using value_type = std::ranges::range_value_t<Container>;
                            return to<value_type>(std::forward<element_type>(elem));
                        }
                    ),
                std::forward<Args>(args)...
            );
        }
    }
}

#endif
