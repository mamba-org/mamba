// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_ENCODING_HPP
#define MAMBA_UTIL_ENCODING_HPP

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <tl/expected.hpp>

namespace mamba::util
{
    enum struct EncodingError
    {
        Ok,
        InvalidInput,
    };

    /**
     * Convert the lower nibble to a hexadecimal representation.
     */
    [[nodiscard]] auto nibble_to_hex(std::byte b) noexcept -> char;

    /**
     * Convert a buffer of bytes to a hexadecimal string written in the @p out parameter.
     *
     * The @p out parameter must be allocated with twice the size of the input byte buffer.
     */
    void bytes_to_hex_to(std::span<const std::byte> bytes, char* out) noexcept;

    /**
     * Convert a buffer of bytes to a hexadecimal string.
     */
    [[nodiscard]] auto bytes_to_hex_str(std::span<const std::byte> bytes) -> std::string;

    /**
     * Escape reserved URL reserved characters with '%' encoding.
     *
     * The second argument can be used to specify characters to exclude from encoding,
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

    /**
     * Convert a ``std::u8string`` to a UTF-8 encoded ``std::string``.
     *
     * We assume here that ``char`` and ``char8_t`` are containing the same Unicode data.
     */
    [[nodiscard]] auto to_utf8_std_string(std::u8string_view text) -> std::string;

    /**
     * Convert a UTF-8 encoded ``std::string`` to a ``std::u8string` .
     *
     * We assume here that ``char`` and ``char8_t`` are containing the same Unicode data.
     * No checks are made.
     */
    [[nodiscard]] auto to_u8string(std::string_view text) -> std::u8string;
}
#endif
