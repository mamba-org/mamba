// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_VALIDATION_TOOLS_HPP
#define MAMBA_VALIDATION_TOOLS_HPP

#include <array>
#include <cstddef>
#include <string>

namespace mamba::fs
{
    class u8path;
}

namespace mamba::validation
{
    [[nodiscard]] auto sha256sum(const fs::u8path& path) -> std::string;

    [[nodiscard]] auto md5sum(const fs::u8path& path) -> std::string;

    auto file_size(const fs::u8path& path, std::uintmax_t validation) -> bool;

    inline constexpr std::size_t MAMBA_SHA256_SIZE_HEX = 64;
    inline constexpr std::size_t MAMBA_SHA256_SIZE_BYTES = 32;
    inline constexpr std::size_t MAMBA_MD5_SIZE_HEX = 32;
    inline constexpr std::size_t MAMBA_MD5_SIZE_BYTES = 16;
    inline constexpr std::size_t MAMBA_ED25519_KEYSIZE_HEX = 64;
    inline constexpr std::size_t MAMBA_ED25519_KEYSIZE_BYTES = 32;
    inline constexpr std::size_t MAMBA_ED25519_SIGSIZE_HEX = 128;
    inline constexpr std::size_t MAMBA_ED25519_SIGSIZE_BYTES = 64;

    auto generate_ed25519_keypair(std::byte* pk, std::byte* sk) -> int;
    auto generate_ed25519_keypair() -> std::pair<
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>,
        std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>>;
    auto generate_ed25519_keypair_hex() -> std::pair<std::string, std::string>;

    auto sign(const std::string& data, const std::byte* sk, std::byte* signature) -> int;
    auto sign(const std::string& data, const std::string& sk, std::string& signature) -> int;

    auto ed25519_sig_hex_to_bytes(const std::string& sig_hex, int& error_code) noexcept
        -> std::array<std::byte, MAMBA_ED25519_SIGSIZE_BYTES>;

    auto ed25519_key_hex_to_bytes(const std::string& key_hex, int& error_code) noexcept
        -> std::array<std::byte, MAMBA_ED25519_KEYSIZE_BYTES>;

    auto
    verify(const std::byte* data, std::size_t data_len, const std::byte* pk, const std::byte* signature)
        -> int;
    auto verify(const std::string& data, const std::byte* pk, const std::byte* signature) -> int;
    auto verify(const std::string& data, const std::string& pk_hex, const std::string& signature_hex)
        -> int;

    /**
     * Verify a GPG/PGP signature against the hash of the binary data and
     * the additional trailer added in V4 signature.
     * See RFC4880, section 5.2.4 https://datatracker.ietf.org/doc/html/rfc4880#section-5.2.4
     * This method assumes hash function to be SHA-256
     */
    auto verify_gpg_hashed_msg(const std::byte* data, const std::byte* pk, const std::byte* signature)
        -> int;
    auto
    verify_gpg_hashed_msg(const std::string& data, const std::byte* pk, const std::byte* signature)
        -> int;
    auto
    verify_gpg_hashed_msg(const std::string& data, const std::string& pk, const std::string& signature)
        -> int;

    /**
     * Verify a GPG/PGP signature against the binary data and
     * the additional trailer added in V4 signature.
     * See RFC4880, section 5.2.4 https://datatracker.ietf.org/doc/html/rfc4880#section-5.2.4
     * This method assumes hash function to be SHA-256
     */
    auto verify_gpg(
        const std::string& data,
        const std::string& gpg_v4_trailer,
        const std::string& pk,
        const std::string& signature
    ) -> int;

    void check_timestamp_metadata_format(const std::string& ts);
}
#endif
