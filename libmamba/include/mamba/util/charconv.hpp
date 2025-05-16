// Copyright (c) 2025, Cppreference.com
//
// Distributed under the terms of the Copyright/CC-BY-SA License.
//
// The full license can be found at the address
// https://en.cppreference.com/w/Cppreference:Copyright/CC-BY-SA

/**
 * Backport of C++23 ``std::from_chars`` function as constexpr.
 */

#ifndef MAMBA_UTIL_CHARCONV_HPP
#define MAMBA_UTIL_CHARCONV_HPP

#include <charconv>
#include <limits>
#include <type_traits>

#include "mamba/util/deprecation.hpp"

namespace mamba::util
{

    template <typename Int>
    MAMBA_DEPRECATED_CXX23 constexpr auto
    constexpr_from_chars(const char* first, const char* last, Int& value) -> std::from_chars_result
    {
        static_assert(
            std::is_integral_v<Int> && std::is_unsigned_v<Int>,
            "Only unsigned integers supported"
        );

        constexpr auto is_digit = [](char c) -> bool { return c >= '0' && c <= '9'; };

        if (first == last)
        {
            return { first, std::errc::invalid_argument };
        }

        value = 0;
        auto it = first;
        while (it != last && is_digit(*it))
        {
            Int digit = static_cast<Int>(static_cast<unsigned char>(*it) - '0');

            if (value > (std::numeric_limits<Int>::max() - digit) / 10)
            {
                return { it, std::errc::result_out_of_range };
            }

            value = value * 10 + digit;
            ++it;
        }

        if (it == first)
        {
            return { first, std::errc::invalid_argument };
        }

        return { it, std::errc{} };
    }
}
#endif
