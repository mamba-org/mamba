// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_UTIL_CRYPTOGRAPHY_HPP
#define MAMBA_UTIL_CRYPTOGRAPHY_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "mamba/util/encoding.hpp"

using EVP_MD_CTX = struct evp_md_ctx_st;  // OpenSSL impl

namespace mamba::util
{
    /**
     * Provide high-level hashing functions over a digest hashing algorithm.
     */
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

        // TODO(C++20): use std::span<std::byte>
        struct blob_type
        {
            const std::byte* data;
            std::size_t size;
        };

        /**
         * Hash a blob of data and write the hashed bytes to the provided output.
         */
        void blob_bytes_to(blob_type blob, std::byte* out);

        /**
         * Hash a blob of data and return the hashed bytes as an array.
         */
        [[nodiscard]] auto blob_bytes(blob_type blob) -> bytes_array;

        /**
         * Hash a blob of data and write the hashed bytes with hexadecimal encoding to the output.
         */
        void blob_hex_to(blob_type blob, char* out);

        /**
         * Hash a blob of data and return the hashed bytes with hexadecimal encoding as an array.
         */
        [[nodiscard]] auto blob_hex(blob_type blob) -> hex_array;

        /**
         * Hash a blob of data and return the hashed bytes with hexadecimal encoding as a string.
         */
        [[nodiscard]] auto blob_hex_str(blob_type blob) -> std::string;

        /**
         * Hash a string and write the hashed bytes to the provided output.
         */
        void str_bytes_to(std::string_view data, std::byte* out);

        /**
         * Hash a string and return the hashed bytes as an array.
         */
        [[nodiscard]] auto str_bytes(std::string_view data) -> bytes_array;

        /**
         * Hash a string and write the hashed bytes with hexadecimal encoding to the output.
         */
        void str_hex_to(std::string_view data, char* out);

        /**
         * Hash a string and return the hashed bytes with hexadecimal encoding as an array.
         */
        [[nodiscard]] auto str_hex(std::string_view data) -> hex_array;

        /**
         * Hash a string and return the hashed bytes with hexadecimal encoding as a string.
         */
        [[nodiscard]] auto str_hex_str(std::string_view data) -> std::string;

        /**
         * Incrementally hash a file and write the hashed bytes to the provided output.
         */
        void file_bytes_to(std::ifstream& file, std::byte* out);

        /**
         * Incrementally hash a file and return the hashed bytes as an array.
         */
        [[nodiscard]] auto file_bytes(std::ifstream& file) -> bytes_array;

        /**
         * Incrementally hash a file and write the hashed bytes with hexadecimal encoding to the
         * output.
         */
        void file_hex_to(std::ifstream& infile, char* out);

        /**
         * Incrementally hash a file and return the hashed bytes with hexadecimal encoding as an
         * array.
         */
        [[nodiscard]] auto file_hex(std::ifstream& file) -> hex_array;

        /**
         * Incrementally hash a file and return the hashed bytes with hexadecimal encoding as a
         * string.
         */
        [[nodiscard]] auto file_hex_str(std::ifstream& file) -> std::string;

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
            void digest_update(const std::byte* buffer, std::size_t count);
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

    class Sha256Digester : private detail::EVPDigester
    {
    public:

        inline static constexpr std::size_t bytes_size = 32;
        inline static constexpr std::size_t digest_size = 32768;

        using detail::EVPDigester::digest_start;
        using detail::EVPDigester::digest_update;
        using detail::EVPDigester::digest_finalize_to;

        Sha256Digester()
            : EVPDigester(detail::EVPDigester::Algorithm::sha256)
        {
        }
    };

    using Sha256Hasher = DigestHasher<Sha256Digester>;

    class Md5Digester : private detail::EVPDigester
    {
    public:

        inline static constexpr std::size_t bytes_size = 16;
        inline static constexpr std::size_t digest_size = 32768;

        using detail::EVPDigester::digest_start;
        using detail::EVPDigester::digest_update;
        using detail::EVPDigester::digest_finalize_to;

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
    void DigestHasher<D>::blob_bytes_to(blob_type blob, std::byte* out)
    {
        m_digester.digest_start();

        auto [iter, remaining] = blob;
        while (remaining > 0)
        {
            const auto taken = std::min(remaining, digest_size);
            m_digester.digest_update(const_cast<std::byte*>(iter), taken);
            remaining -= taken;
            iter += taken;
        }
        return m_digester.digest_finalize_to(out);
    }

    template <typename D>
    auto DigestHasher<D>::blob_bytes(blob_type blob) -> bytes_array
    {
        auto out = bytes_array{};
        blob_bytes_to(blob, out.data());
        return out;
    }

    template <typename D>
    void DigestHasher<D>::blob_hex_to(blob_type blob, char* out)
    {
        // Reusing the output array to write the temporary bytes
        static_assert(hex_size >= 2 * bytes_size);
        static_assert(sizeof(std::byte) == sizeof(char));
        auto bytes_first = reinterpret_cast<std::byte*>(out) + bytes_size;
        auto bytes_last = bytes_first + bytes_size;
        blob_bytes_to(blob, bytes_first);
        bytes_to_hex_to(bytes_first, bytes_last, out);
    }

    template <typename D>
    auto DigestHasher<D>::blob_hex(blob_type blob) -> hex_array
    {
        auto out = hex_array{};
        blob_hex_to(blob, out.data());
        return out;
    }

    template <typename D>
    auto DigestHasher<D>::blob_hex_str(blob_type blob) -> std::string
    {
        auto out = std::string(hex_size, 'x');  // An invalid character
        blob_hex_to(blob, out.data());
        return out;
    }

    template <typename D>
    void DigestHasher<D>::str_bytes_to(std::string_view data, std::byte* out)
    {
        blob_bytes_to({ reinterpret_cast<const std::byte*>(data.data()), data.size() }, out);
    }

    template <typename D>
    auto DigestHasher<D>::str_bytes(std::string_view data) -> bytes_array
    {
        return blob_bytes({ reinterpret_cast<const std::byte*>(data.data()), data.size() });
    }

    template <typename D>
    void DigestHasher<D>::str_hex_to(std::string_view data, char* out)
    {
        blob_hex_to({ reinterpret_cast<const std::byte*>(data.data()), data.size() }, out);
    }

    template <typename D>
    auto DigestHasher<D>::str_hex(std::string_view data) -> hex_array
    {
        return blob_hex({ reinterpret_cast<const std::byte*>(data.data()), data.size() });
    }

    template <typename D>
    auto DigestHasher<D>::str_hex_str(std::string_view data) -> std::string
    {
        return blob_hex_str({ reinterpret_cast<const std::byte*>(data.data()), data.size() });
    }

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
    void DigestHasher<D>::file_hex_to(std::ifstream& infile, char* out)
    {
        // Reusing the output array to write the temporary bytes
        static_assert(hex_size >= 2 * bytes_size);
        static_assert(sizeof(std::byte) == sizeof(char));
        auto bytes_first = reinterpret_cast<std::byte*>(out) + bytes_size;
        auto bytes_last = bytes_first + bytes_size;
        file_bytes_to(infile, bytes_first);
        bytes_to_hex_to(bytes_first, bytes_last, out);
    }

    template <typename D>
    auto DigestHasher<D>::file_hex(std::ifstream& infile) -> hex_array
    {
        auto out = hex_array{};
        file_hex_to(infile, out.data());
        return out;
    }

    template <typename D>
    auto DigestHasher<D>::file_hex_str(std::ifstream& infile) -> std::string
    {
        auto out = std::string(hex_size, 'x');  // An invalid character
        file_hex_to(infile, out.data());
        return out;
    }
}
#endif
