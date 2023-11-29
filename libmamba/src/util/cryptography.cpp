// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cassert>
#include <fstream>
#include <memory>
#include <vector>

#include <openssl/evp.h>

#include "mamba/util/cryptography.hpp"

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

    namespace
    {
        struct EVPContextDeleter
        {
            using value_type = ::EVP_MD_CTX;
            using pointer_type = value_type*;

            void operator()(pointer_type ptr) const
            {
                if (ptr)
                {
                    ::EVP_MD_CTX_destroy(ptr);
                }
            }
        };

        auto make_EVP_context()
        {
            auto ptr = std::unique_ptr<::EVP_MD_CTX, EVPContextDeleter>();
            ptr.reset(::EVP_MD_CTX_create());
            return ptr;
        }

    }

    void
    sha256bytes_file_to(std::ifstream& infile, std::byte* out, std::vector<std::byte>& tmp_buffer)
    {
        assert(infile.good());

        static constexpr std::size_t EVP_BUFSIZE = 32768;
        tmp_buffer.assign(EVP_BUFSIZE, std::byte(0));

        auto mdctx = make_EVP_context();
        ::EVP_DigestInit_ex(mdctx.get(), EVP_sha256(), nullptr);

        while (infile)
        {
            infile.read(reinterpret_cast<char*>(tmp_buffer.data()), EVP_BUFSIZE);
            const auto count = static_cast<std::size_t>(infile.gcount());
            if (!count)
            {
                break;
            }
            ::EVP_DigestUpdate(mdctx.get(), tmp_buffer.data(), count);
        }

        ::EVP_DigestFinal_ex(mdctx.get(), reinterpret_cast<unsigned char*>(out), nullptr);
    }

    void sha256bytes_file_to(std::ifstream& infile, std::byte* out)
    {
        auto buffer = std::vector<std::byte>();
        return sha256bytes_file_to(infile, out, buffer);
    }

    auto sha256bytes_file(std::ifstream& infile) -> sha256bytes_array
    {
        auto out = sha256bytes_array{};
        sha256bytes_file_to(infile, out.data());
        return out;
    }

    auto sha256hex_file(std::ifstream& infile) -> sha256hex_array
    {
        auto out = sha256hex_array{};

        // Reusing the output array to write the temprary bytes
        assert(out.size() == 2 * SHA256_SIZE_BYTES);
        auto bytes_first = reinterpret_cast<std::byte*>(out.data()) + SHA256_SIZE_BYTES;
        auto bytes_last = bytes_first + SHA256_SIZE_BYTES;

        sha256bytes_file_to(infile, bytes_first);

        bytes_to_hex_to(bytes_first, bytes_last, out.data());
        return out;
    }
}
