// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_ENCODING_HPP
#define MAMBA_UTIL_ENCODING_HPP

#include <cstddef>
#include <string>
#include <string_view>

#include <tl/expected.hpp>

namespace mamba::util
{
    struct EncodingError
    {
    };

    /**
     * Convert a buffer of bytes to a hexadecimal string written in the @p out paremeter.
     *
     * The @p out parameter must be allocated with twice the size of the input byte buffer.
     */
    void bytes_to_hex_to(const std::byte* first, const std::byte* last, char* out);

    /**
     * Convert a buffer of bytes to a hexadecimal string.
     */
    [[nodiscard]] auto bytes_to_hex_str(const std::byte* first, const std::byte* last) -> std::string;

    /**
     * Escape reserved URL reserved characters with '%' encoding.
     *
     * The secons argument can be used to specify characters to exclude from encoding,
     * so that for instance path can be encoded without splitting them (if they have no '/' other
     * than separators).
     *
     * @see url_decode
     */
    [[nodiscard]] auto encode_percent(std::string_view url) -> std::string;
    [[nodiscard]] auto encode_percent(std::string_view url, std::string_view exclude) -> std::string;
    [[nodiscard]] auto encode_percent(std::string_view url, char exclude) -> std::string;

    /**
     * Unescape percent encoded string to their URL reserved characters.
     *
     * @see encode_percent
     */
    [[nodiscard]] auto decode_percent(std::string_view url) -> std::string;

    /**
     * Convert a string to base64 encoding.
     */
    [[nodiscard]] auto encode_base64(std::string_view input)
        -> tl::expected<std::string, EncodingError>;

    /**
     * Convert a string from base64 back to its original representation.
     */
    [[nodiscard]] auto decode_base64(std::string_view input)
        -> tl::expected<std::string, EncodingError>;
}
#endif
