// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cstdint>

#include "mamba/util/encoding.hpp"

namespace mamba::util
{
    // TODO(C++20): use std::span
    void bytes_to_hex_to(const std::byte* first, const std::byte* last, char* out)
    {
        constexpr auto hex_chars = std::array{ '0', '1', '2', '3', '4', '5', '6', '7',
                                               '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
        while (first != last)
        {
            const auto byte = static_cast<std::uint8_t>(*first);
            *out++ = hex_chars[byte >> 4];
            *out++ = hex_chars[byte & 0xF];
            ++first;
        }
    }
}
