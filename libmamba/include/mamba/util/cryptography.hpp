// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_CRYPTOGRAPHY_HPP
#define MAMBA_UTIL_CRYPTOGRAPHY_HPP

#include <array>
#include <cstddef>
#include <fstream>
#include <memory>
#include <vector>

using EVP_MD_CTX = struct evp_md_ctx_st;

namespace mamba::util
{
    void bytes_to_hex_to(const std::byte* first, const std::byte* last, char* out);

    template <typename Digester>
    class DigestHasher
    {
    public:

        using digester_type = Digester;

        inline static constexpr std::size_t bytes_size = digester_type::bytes_size;
        inline static constexpr std::size_t hex_size = 2 * bytes_size;
        inline static constexpr std::size_t digest_size = digester_type::digest_size;

        using bytes_array = std::array<std::byte, bytes_size>;
        using hex_array = std::array<char, hex_size>;

        void file_bytes_to(std::ifstream& file, std::byte* out);

        [[nodiscard]] auto file_bytes(std::ifstream& file) -> bytes_array;

        [[nodiscard]] auto file_hex(std::ifstream& file) -> hex_array;

    private:

        std::vector<std::byte> m_digest_buffer = {};
        digester_type m_digester = {};
    };

    namespace detail
    {
        class EVPDigester
        {
        public:

            enum struct Algorithm
            {
                sha256,
                md5
            };

            EVPDigester(Algorithm algo);

            void digest_start();
            void digest_update(std::byte* buffer, std::size_t count);
            void digest_finalize_to(std::byte* hash);

        private:

            struct EVPContextDeleter
            {
                void operator()(::EVP_MD_CTX* ptr) const;
            };

            std::unique_ptr<::EVP_MD_CTX, EVPContextDeleter> m_ctx;
            Algorithm m_algorithm;
        };
    }

    class Sha256Digester : public detail::EVPDigester
    {
    public:

        inline static constexpr std::size_t bytes_size = 32;
        inline static constexpr std::size_t digest_size = 32768;

        Sha256Digester()
            : EVPDigester(detail::EVPDigester::Algorithm::sha256)
        {
        }
    };

    using Sha256Hasher = DigestHasher<Sha256Digester>;

    class Md5Digester : public detail::EVPDigester
    {
    public:

        inline static constexpr std::size_t bytes_size = 16;
        inline static constexpr std::size_t digest_size = 32768;

        Md5Digester()
            : EVPDigester(detail::EVPDigester::Algorithm::md5)
        {
        }
    };

    using Md5Hasher = DigestHasher<Md5Digester>;

    /************************************
     *  Implementation of DigestHasher  *
     ************************************/

    template <typename D>
    void DigestHasher<D>::file_bytes_to(std::ifstream& infile, std::byte* out)
    {
        m_digest_buffer.assign(digest_size, std::byte(0));
        m_digester.digest_start();

        while (infile)
        {
            infile.read(reinterpret_cast<char*>(m_digest_buffer.data()), digest_size);
            const auto count = static_cast<std::size_t>(infile.gcount());
            if (!count)
            {
                break;
            }
            m_digester.digest_update(m_digest_buffer.data(), count);
        }
        return m_digester.digest_finalize_to(out);
    }

    template <typename D>
    auto DigestHasher<D>::file_bytes(std::ifstream& infile) -> bytes_array
    {
        auto out = bytes_array{};
        file_bytes_to(infile, out.data());
        return out;
    }

    template <typename D>
    auto DigestHasher<D>::file_hex(std::ifstream& infile) -> hex_array
    {
        auto out = hex_array{};

        // Reusing the output array to write the temporary bytes
        static_assert(hex_size >= 2 * bytes_size);
        auto bytes_first = reinterpret_cast<std::byte*>(out.data()) + bytes_size;
        auto bytes_last = bytes_first + bytes_size;
        file_bytes_to(infile, bytes_first);

        bytes_to_hex_to(bytes_first, bytes_last, out.data());
        return out;
    }
}
#endif
