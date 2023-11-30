// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <cassert>
#include <memory>

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

    namespace detail
    {
        void EVPDigester::EVPContextDeleter::operator()(::EVP_MD_CTX* ptr) const
        {
            if (ptr)
            {
                ::EVP_MD_CTX_destroy(ptr);
            }
        }

        EVPDigester::EVPDigester(Algorithm algo)
            : m_algorithm(algo)
        {
            m_ctx.reset(::EVP_MD_CTX_create());
        }

        void EVPDigester::digest_start()
        {
            [[maybe_unused]] int status = 0;
            switch (m_algorithm)
            {
                case (Algorithm::sha256):
                {
                    status = ::EVP_DigestInit_ex(m_ctx.get(), EVP_sha256(), nullptr);
                    break;
                }
                case (Algorithm::md5):
                {
                    status = ::EVP_DigestInit_ex(m_ctx.get(), EVP_md5(), nullptr);
                    break;
                }
            }
            assert(status != 0);
        }

        void EVPDigester::digest_update(std::byte* buffer, std::size_t count)
        {
            ::EVP_DigestUpdate(m_ctx.get(), buffer, count);
        }

        void EVPDigester::digest_finalize_to(std::byte* hash)
        {
            ::EVP_DigestFinal_ex(m_ctx.get(), reinterpret_cast<unsigned char*>(hash), nullptr);
        }
    }
}
