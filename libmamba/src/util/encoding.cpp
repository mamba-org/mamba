// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cstdint>
#include <cstring>

#include <openssl/evp.h>

#include "mamba/util/compare.hpp"
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

    auto encode_base64(std::string_view input) -> tl::expected<std::string, EncodingError>
    {
        const auto expected_size = 4 * ((input.size() + 2) / 3);
        auto out = std::string(expected_size, '#');  // Invalid char
        const auto written_size = ::EVP_EncodeBlock(
            reinterpret_cast<unsigned char*>(out.data()),
            reinterpret_cast<const unsigned char*>(input.data()),
            static_cast<int>(input.size())
        );

        if (util::cmp_not_equal(expected_size, written_size))
        {
            return tl::make_unexpected(EncodingError());
        }
        return { std::move(out) };
    }

    auto decode_base64(std::string_view input) -> tl::expected<std::string, EncodingError>
    {
        const auto max_expected_size = 3 * input.size() / 4;
        auto out = std::string(max_expected_size, 'x');
        // Writes the string and the null terminator
        const auto max_possible_written_size = ::EVP_DecodeBlock(
            reinterpret_cast<unsigned char*>(out.data()),
            reinterpret_cast<const unsigned char*>(input.data()),
            static_cast<int>(input.size())
        );
        if (util::cmp_not_equal(max_expected_size, max_possible_written_size))
        {
            return tl::make_unexpected(EncodingError());
        }

        // Sometimes the number reported/computed is smaller than the actual length.
        auto min_expected_size = static_cast<std::size_t>(std::max(max_possible_written_size, 4) - 4);
        auto extra = std::strlen(out.c_str() + min_expected_size);
        out.resize(min_expected_size + extra);
        return { std::move(out) };
    }
}
