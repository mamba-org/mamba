// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <array>
#include <string_view>

#include <doctest/doctest.h>

#include "mamba/util/cryptography.hpp"

#include "doctest-printer/array.hpp"
#include "doctest-printer/optional.hpp"

using namespace mamba::util;

TEST_SUITE("util::cryptography")
{
    TEST_CASE("bytes_to_hex")
    {
        constexpr auto bytes = std::array{
            std::byte{ 0x00 }, std::byte{ 0x01 }, std::byte{ 0x03 }, std::byte{ 0x09 },
            std::byte{ 0x0A }, std::byte{ 0x0D }, std::byte{ 0x0F }, std::byte{ 0xAD },
            std::byte{ 0x10 }, std::byte{ 0x30 }, std::byte{ 0xA0 }, std::byte{ 0xD0 },
            std::byte{ 0xF0 }, std::byte{ 0xAD }, std::byte{ 0xA9 }, std::byte{ 0x4E },
            std::byte{ 0xEF }, std::byte{ 0xFF },
        };

        auto hex = std::array<char, bytes.size() * 2>{};
        bytes_to_hex_to(bytes.data(), bytes.data() + bytes.size(), hex.data());
        const auto hex_str = std::string_view(hex.data(), hex.size());
        CHECK_EQ(hex_str, "000103090a0d0fad1030a0d0f0ada94eefff");
    }
}