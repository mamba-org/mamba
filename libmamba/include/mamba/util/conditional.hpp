// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_CONDITIONAL_HPP
#define MAMBA_UTIL_CONDITIONAL_HPP

#include <type_traits>

namespace mamba::util
{

    template <typename Int>
    [[nodiscard]] auto if_else(bool condition, Int true_val, Int false_val) noexcept -> Int;

    /********************
     *  Implementation  *
     ********************/

    template <typename Int>
    auto if_else(bool condition, Int true_val, Int false_val) noexcept -> Int
    {
        if constexpr (std::is_enum_v<Int>)
        {
            using int_t = std::underlying_type_t<Int>;
            return static_cast<Int>(
                if_else(condition, static_cast<int_t>(true_val), static_cast<int_t>(false_val))
            );
        }
        else
        {
            return condition ? true_val : false_val;
        }
    }
}
#endif
