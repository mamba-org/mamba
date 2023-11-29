// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_CRYPTOGRAPHY_HPP
#define MAMBA_UTIL_CRYPTOGRAPHY_HPP

#include <array>
#include <cstddef>
#include <iosfwd>
#include <vector>

namespace mamba::util
{
    void bytes_to_hex_to(const std::byte* first, const std::byte* last, char* out);

    inline constexpr std::size_t SHA256_SIZE_BYTES = 32;
    using sha256bytes_array = std::array<std::byte, SHA256_SIZE_BYTES>;
    inline constexpr std::size_t SHA256_SIZE_HEX = 64;
    using sha256hex_array = std::array<char, SHA256_SIZE_HEX>;

    void sha256bytes_file_to(std::ifstream& file, std::byte* out, std::vector<std::byte>& tmp_buffer);

    void sha256bytes_file_to(std::ifstream& file, std::byte* out);

    [[nodiscard]] auto sha256bytes_file(std::ifstream& file) -> sha256bytes_array;

    [[nodiscard]] auto sha256hex_file(std::ifstream& file) -> sha256hex_array;
}
#endif
