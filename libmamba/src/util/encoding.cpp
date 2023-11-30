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
#include "mamba/util/string.hpp"

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

    // TODO(C++20): use std::span
    auto bytes_to_hex_str(const std::byte* first, const std::byte* last) -> std::string
    {
        auto out = std::string(static_cast<std::size_t>(last - first) * 2, 'x');
        bytes_to_hex_to(first, last, out.data());
        return out;
    }

    namespace
    {
        auto url_is_unreserved_char(char c) -> bool
        {
            // https://github.com/curl/curl/blob/67e9e3cb1ea498cb94071dddb7653ab5169734b2/lib/escape.c#L45
            return util::is_alphanum(c) || (c == '-') || (c == '.') || (c == '_') || (c == '~');
        }

        auto encode_percent_char(char c) -> std::array<char, 3>
        {
            // https://github.com/curl/curl/blob/67e9e3cb1ea498cb94071dddb7653ab5169734b2/lib/escape.c#L107
            static constexpr auto hex = std::array{
                '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
            };
            return std::array{
                '%',
                hex[static_cast<unsigned char>(c) >> 4],
                hex[c & 0xf],
            };
        }

        auto url_is_hex_char(char c) -> bool
        {
            return util::is_digit(c) || (('A' <= c) && (c <= 'F')) || (('a' <= c) && (c <= 'f'));
        }

        auto url_decode_char(char d10, char d1) -> char
        {
            // https://github.com/curl/curl/blob/67e9e3cb1ea498cb94071dddb7653ab5169734b2/lib/escape.c#L147
            // Offset from char '0', contains '0' padding for incomplete values.
            static constexpr std::array<unsigned char, 55> hex_offset = {
                0, 1,  2,  3,  4,  5,  6,  7, 8, 9, 0, 0, 0, 0, 0, 0, /* 0x30 - 0x3f */
                0, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x40 - 0x4f */
                0, 0,  0,  0,  0,  0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, /* 0x50 - 0x5f */
                0, 10, 11, 12, 13, 14, 15                             /* 0x60 - 0x66 */
            };
            assert('0' <= d10);
            assert('0' <= d1);
            const auto idx10 = static_cast<unsigned char>(d10 - '0');
            const auto idx1 = static_cast<unsigned char>(d1 - '0');
            assert(idx10 < hex_offset.size());
            assert(idx1 < hex_offset.size());
            return static_cast<char>((hex_offset[idx10] << 4) | hex_offset[idx1]);
        }

        template <typename Str>
        auto encode_percent_impl(std::string_view url, Str exclude) -> std::string
        {
            std::string out = {};
            out.reserve(url.size());
            for (char c : url)
            {
                if (url_is_unreserved_char(c) || contains(exclude, c))
                {
                    out += c;
                }
                else
                {
                    const auto encoding = encode_percent_char(c);
                    out += std::string_view(encoding.data(), encoding.size());
                }
            }
            return out;
        }
    }

    auto decode_percent(std::string_view url) -> std::string
    {
        std::string out = {};
        out.reserve(url.size());
        const auto end = url.cend();
        for (auto iter = url.cbegin(); iter < end; ++iter)
        {
            if (((iter + 2) < end) && (iter[0] == '%') && url_is_hex_char(iter[1])
                && url_is_hex_char(iter[2]))
            {
                out.push_back(url_decode_char(iter[1], iter[2]));
                iter += 2;
            }
            else
            {
                out.push_back(*iter);
            }
        }
        return out;
    }

    auto encode_percent(std::string_view url) -> std::string
    {
        return encode_percent_impl(url, 'a');  // Already not encoded
    }

    auto encode_percent(std::string_view url, std::string_view exclude) -> std::string
    {
        return encode_percent_impl(url, exclude);
    }

    auto encode_percent(std::string_view url, char exclude) -> std::string
    {
        return encode_percent_impl(url, exclude);
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
