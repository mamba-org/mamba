// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <utility>

#include <catch2/catch_all.hpp>

#include "mamba/core/util.hpp"
#include "mamba/util/cryptography.hpp"

using namespace mamba::util;

TEST_CASE("Hasher")
{
    const auto known_sha256 = std::array<std::pair<std::string, std::string>, 5>{ {
        {
            "test",
            "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
        },
        {
            "test",
            "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08",
        },
        {
            "This is a string !",
            "4cad2018bf50bdc5c00a0dafdc53e15867c46c8d6cd6dec04302707a5892854e",
        },
        {
            std::string(Sha256Digester::digest_size, 'y'),
            "65be48e7ef751399d65711c5c053c6cec0c412ea22fae85872c867336b955a46",
        },
        {
            std::string(Sha256Digester::digest_size * 2 + 10, 'z'),
            "5fb97842061800e4140e9bb07e161a47d327f023a1e4410ea5af2132449a5b0c",
        },
    } };

    const auto known_md5 = std::array<std::pair<std::string, std::string>, 5>{ {
        { "test", "098f6bcd4621d373cade4e832627b4f6" },
        { "test", "098f6bcd4621d373cade4e832627b4f6" },
        { "This is a string !", "ffadac0192824b39afda20319ba016b6" },
        {
            std::string(Md5Digester::digest_size, 'y'),
            "358b0ef0cd11424177adb11be4b43269",
        },
        {
            std::string(Md5Digester::digest_size * 2 + 10, 'z'),
            "43856e090ea42b2862bcf4b49aeda79f",
        },

    } };

    SECTION("Hash string")
    {
        SECTION("sha256")
        {
            auto reused_hasher = Sha256Hasher();
            for (auto [data, hash] : known_sha256)
            {
                REQUIRE(reused_hasher.str_hex_str(data) == hash);
                auto new_hasher = Sha256Hasher();
                REQUIRE(new_hasher.str_hex_str(data) == hash);
            }
        }

        SECTION("md5")
        {
            auto reused_hasher = Md5Hasher();
            for (auto [data, hash] : known_md5)
            {
                REQUIRE(reused_hasher.str_hex_str(data) == hash);
                auto new_hasher = Md5Hasher();
                REQUIRE(new_hasher.str_hex_str(data) == hash);
            }
        }
    }

    SECTION("Hash file")
    {
        SECTION("sha256")
        {
            auto hasher = Sha256Hasher();
            for (auto [data, hash] : known_sha256)
            {
                auto tmp = mamba::TemporaryFile();
                {
                    auto file = std::fstream(tmp.path().std_path());
                    REQUIRE(file.good());
                    file << data;
                    file.close();
                }
                auto file = std::ifstream(tmp.path().std_path());
                REQUIRE(file.good());
                REQUIRE(hasher.file_hex_str(file) == hash);
            }
        }

        SECTION("md5")
        {
            auto hasher = Md5Hasher();
            for (auto [data, hash] : known_md5)
            {
                auto tmp = mamba::TemporaryFile();
                {
                    auto file = std::fstream(tmp.path().std_path());
                    REQUIRE(file.good());
                    file << data;
                    file.close();
                }
                auto file = std::ifstream(tmp.path().std_path());
                REQUIRE(file.good());
                REQUIRE(hasher.file_hex_str(file) == hash);
            }
        }
    }
}
