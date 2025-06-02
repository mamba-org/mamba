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
    void bytes_to_hex_to(const std::byte* first, const std::byte* last, char* out) noexcept;

    /**
     * Convert a buffer of bytes to a hexadecimal string.
     */
    [[nodiscard]] auto bytes_to_hex_str(const std::byte* first, const std::byte* last) -> std::string;

    /**
     * Convert a hexadecimal character to a lower nibble.
     */
    [[nodiscard]] auto hex_to_nibble(char c, EncodingError& error) noexcept -> std::byte;

    /**
     * Convert a hexadecimal character to a lower nibble.
     */
    [[nodiscard]] auto hex_to_nibble(char c) noexcept -> tl::expected<std::byte, EncodingError>;

    /**
     * Convert two hexadecimal characters to a byte.
     */
    [[nodiscard]] auto two_hex_to_byte(char high, char low, EncodingError& error) noexcept
        -> std::byte;

    /**
     * Convert two hexadecimal characters to a byte.
     */
    [[nodiscard]] auto two_hex_to_byte(char high, char low) noexcept
        -> tl::expected<std::byte, EncodingError>;

    /**
     * Convert hexadecimal characters to a bytes and write it to the given output.
     *
     * The number of hexadecimal characters must be even and out must be allocated with half the
     * number of hexadecimal characters.
     */
    void hex_to_bytes_to(std::string_view hex, std::byte* out, EncodingError& error) noexcept;

    /**
     * Convert hexadecimal characters to a bytes and write it to the given output.
     *
     * The number of hexadecimal characters must be even and out must be allocated with half the
     * number of hexadecimal characters.
     */
    [[nodiscard]] auto hex_to_bytes_to(std::string_view hex, std::byte* out) noexcept
        -> tl::expected<void, EncodingError>;

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
