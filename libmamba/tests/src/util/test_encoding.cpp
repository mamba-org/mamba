// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.


#include <array>
#include <string_view>

#include <doctest/doctest.h>

#include "mamba/util/encoding.hpp"

#include "doctest-printer/array.hpp"

using namespace mamba::util;

TEST_SUITE("util::encoding")
{
    TEST_CASE("Hexadecimal")
    {
        SUBCASE("nibble_to_hex")
        {
            CHECK_EQ(nibble_to_hex(std::byte{ 0x00 }), '0');
            CHECK_EQ(nibble_to_hex(std::byte{ 0x10 }), '0');  // high ignored
            CHECK_EQ(nibble_to_hex(std::byte{ 0x01 }), '1');
            CHECK_EQ(nibble_to_hex(std::byte{ 0x0D }), 'd');
        }

        SUBCASE("bytes_to_hex_to")
        {
            constexpr auto bytes = std::array{
                std::byte{ 0x00 }, std::byte{ 0x01 }, std::byte{ 0x03 }, std::byte{ 0x09 },
                std::byte{ 0x0A }, std::byte{ 0x0D }, std::byte{ 0x0F }, std::byte{ 0xAD },
                std::byte{ 0x10 }, std::byte{ 0x30 }, std::byte{ 0xA0 }, std::byte{ 0xD0 },
                std::byte{ 0xF0 }, std::byte{ 0xAD }, std::byte{ 0xA9 }, std::byte{ 0x4E },
                std::byte{ 0xEF }, std::byte{ 0xFF },
            };

            CHECK_EQ(
                bytes_to_hex_str(bytes.data(), bytes.data() + bytes.size()),
                "000103090a0d0fad1030a0d0f0ada94eefff"
            );
        }

        SUBCASE("hex_to_nibble")
        {
            CHECK_EQ(hex_to_nibble('0').value(), std::byte{ 0x00 });
            CHECK_EQ(hex_to_nibble('a').value(), std::byte{ 0x0A });
            CHECK_EQ(hex_to_nibble('f').value(), std::byte{ 0x0F });
            CHECK_EQ(hex_to_nibble('B').value(), std::byte{ 0x0B });

            CHECK_FALSE(hex_to_nibble('x').has_value());
            CHECK_FALSE(hex_to_nibble('*').has_value());
            CHECK_FALSE(hex_to_nibble('\0').has_value());
            CHECK_FALSE(hex_to_nibble('~').has_value());
        }

        SUBCASE("two_hex_to_byte")
        {
            CHECK_EQ(two_hex_to_byte('0', '0').value(), std::byte{ 0x00 });
            CHECK_EQ(two_hex_to_byte('0', '4').value(), std::byte{ 0x04 });
            CHECK_EQ(two_hex_to_byte('5', '0').value(), std::byte{ 0x50 });
            CHECK_EQ(two_hex_to_byte('F', 'F').value(), std::byte{ 0xFF });
            CHECK_EQ(two_hex_to_byte('0', 'A').value(), std::byte{ 0x0A });
            CHECK_EQ(two_hex_to_byte('b', '8').value(), std::byte{ 0xB8 });

            CHECK_FALSE(two_hex_to_byte('b', 'x').has_value());
            CHECK_FALSE(two_hex_to_byte('!', 'b').has_value());
            CHECK_FALSE(two_hex_to_byte(' ', '~').has_value());
        }

        SUBCASE("hex_to_bytes")
        {
            using bytes = std::vector<std::byte>;

            SUBCASE("1234")
            {
                auto str = std::string_view("1234");
                auto b = bytes(str.size() / 2);
                REQUIRE(hex_to_bytes_to(str, b.data()).has_value());
                CHECK_EQ(b, bytes{ std::byte{ 0x12 }, std::byte{ 0x34 } });
            }

            SUBCASE("1f4DaB")
            {
                auto str = std::string_view("1f4DaB");
                auto b = bytes(str.size() / 2);
                REQUIRE(hex_to_bytes_to(str, b.data()).has_value());
                CHECK_EQ(b, bytes{ std::byte{ 0x1F }, std::byte{ 0x4D }, std::byte{ 0xAB } });
            }

            SUBCASE("1f4Da")
            {
                // Odd number
                auto str = std::string_view("1f4Da");
                auto b = bytes(str.size() / 2);
                REQUIRE_FALSE(hex_to_bytes_to(str, b.data()).has_value());
            }

            SUBCASE("1fx4")
            {
                // Bad hex
                auto str = std::string_view("1fx4");
                auto b = bytes(str.size() / 2);
                REQUIRE_FALSE(hex_to_bytes_to(str, b.data()).has_value());
            }
        }
    }

    TEST_CASE("percent")
    {
        SUBCASE("encode")
        {
            CHECK_EQ(encode_percent(""), "");
            CHECK_EQ(encode_percent("page"), "page");
            CHECK_EQ(encode_percent(" /word%"), "%20%2Fword%25");
            CHECK_EQ(encode_percent("user@email.com"), "user%40email.com");
            CHECK_EQ(
                encode_percent(R"(#!$&'"(ab23)*+,/:;=?@[])"),
                "%23%21%24%26%27%22%28ab23%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"
            );
            // Does NOT parse URL
            CHECK_EQ(encode_percent("https://foo/"), "https%3A%2F%2Ffoo%2F");

            // Exclude characters
            CHECK_EQ(encode_percent(" /word%", '/'), "%20/word%25");
        }

        SUBCASE("decode")
        {
            CHECK_EQ(decode_percent(""), "");
            CHECK_EQ(decode_percent("page"), "page");
            CHECK_EQ(decode_percent("%20%2Fword%25"), " /word%");
            CHECK_EQ(decode_percent(" /word%25"), " /word%");
            CHECK_EQ(decode_percent("user%40email.com"), "user@email.com");
            CHECK_EQ(decode_percent("https%3A%2F%2Ffoo%2F"), "https://foo/");
            CHECK_EQ(
                decode_percent("%23%21%24%26%27%22%28ab23%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"),
                R"(#!$&'"(ab23)*+,/:;=?@[])"
            );
        }
    }

    TEST_CASE("base64")
    {
        SUBCASE("encode")
        {
            CHECK_EQ(encode_base64("Hello").value(), "SGVsbG8=");
            CHECK_EQ(encode_base64("Hello World!").value(), "SGVsbG8gV29ybGQh");
            CHECK_EQ(encode_base64("!@#$%^U&I*O").value(), "IUAjJCVeVSZJKk8=");
            CHECK_EQ(
                encode_base64(u8"_私のにほHelloわへたです").value(),
                "X+engeOBruOBq+OBu0hlbGxv44KP44G444Gf44Gn44GZ"
            );
            CHECK_EQ(encode_base64("xyzpass").value(), "eHl6cGFzcw==");
        }

        SUBCASE("decode")
        {
            CHECK_EQ(decode_base64("SGVsbG8=").value(), "Hello");
            CHECK_EQ(decode_base64("SGVsbG8gV29ybGQh").value(), "Hello World!");
            CHECK_EQ(decode_base64("IUAjJCVeVSZJKk8=").value(), "!@#$%^U&I*O");
            CHECK_EQ(
                decode_base64(u8"X+engeOBruOBq+OBu0hlbGxv44KP44G444Gf44Gn44GZ").value(),
                "_私のにほHelloわへたです"
            );
            CHECK_EQ(decode_base64("eHl6cGFzcw==").value(), "xyzpass");
        }
    }
}
