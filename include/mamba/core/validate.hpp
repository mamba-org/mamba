// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_VALIDATE_HPP
#define MAMBA_CORE_VALIDATE_HPP

#include <string>

#include "mamba_fs.hpp"


namespace validate
{
    std::string sha256sum(const std::string& path);
    std::string md5sum(const std::string& path);
    bool sha256(const std::string& path, const std::string& validation);
    bool md5(const std::string& path, const std::string& validation);
    bool file_size(const fs::path& path, std::uintmax_t validation);

    int hex2bin(unsigned char* const bin,
                const size_t bin_maxlen,
                const char* const hex,
                const size_t hex_len,
                const char* const ignore,
                size_t* const bin_len,
                const char** const hex_end);
    char* bin2hex(char* const hex,
                  const size_t hex_maxlen,
                  const unsigned char* const bin,
                  const size_t bin_len);

    const std::size_t MAMBA_SHA256_SIZE_HEX = 64;
    const std::size_t MAMBA_SHA256_SIZE_BYTES = 32;
    const std::size_t MAMBA_ED25519_KEYSIZE_HEX = 64;
    const std::size_t MAMBA_ED25519_KEYSIZE_BYTES = 32;
    const std::size_t MAMBA_ED25519_SIGSIZE_HEX = 128;
    const std::size_t MAMBA_ED25519_SIGSIZE_BYTES = 64;

    int generate_ed25519_keypair(unsigned char* pk, unsigned char* sk);

    int sign(const std::string& data, const unsigned char* sk, unsigned char* signature);

    int verify(const unsigned char* data,
               std::size_t data_len,
               unsigned char* pk,
               unsigned char* signature);
    int verify(const std::string& data, unsigned char* pk, unsigned char* signature);
    int verify(const std::string& data, const std::string& pk, const std::string& signature);

    int verify_gpg_hashed_msg(const std::string& data, unsigned char* pk, unsigned char* signature);
    int verify_gpg_hashed_msg(const std::string& data,
                              const std::string& pk,
                              const std::string& signature);
}  // namespace validate

#endif  // MAMBA_VALIDATE_HPP
