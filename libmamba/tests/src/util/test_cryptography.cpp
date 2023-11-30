// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <array>
#include <string_view>

#include <doctest/doctest.h>

#include "mamba/core/util.hpp"
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

    TEST_CASE("Hasher")
    {
        SUBCASE("Hash file")
        {
            auto tmp = mamba::TemporaryFile();
            {
                auto file = std::fstream(tmp.path());
                REQUIRE(file.good());
                file << "test";
                file.close();
            }
            auto file = std::ifstream(tmp.path());
            REQUIRE(file.good());

            SUBCASE("sha256")
            {
                auto hasher = Sha256Hasher();
                auto hash_hex = hasher.file_hex(file);
                auto hash_hex_str = std::string_view(hash_hex.data(), hash_hex.size());
                CHECK_EQ(
                    hash_hex_str,
                    "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"
                );
            }

            SUBCASE("md5")
            {
                auto hasher = Md5Hasher();
                auto hash_hex = hasher.file_hex(file);
                auto hash_hex_str = std::string_view(hash_hex.data(), hash_hex.size());
                CHECK_EQ(hash_hex_str, "098f6bcd4621d373cade4e832627b4f6");
            }
        }
    }
}
