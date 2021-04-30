// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/core/validate.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"

#include "openssl/md5.h"
#include "openssl/sha.h"

#include "sodium/crypto_sign.h"
#include "sodium/crypto_hash_sha256.h"
#include "sodium/utils.h"

#include <vector>
#include <stdexcept>
#include <iostream>


namespace validate
{
    std::string sha256sum(const std::string& path)
    {
        std::array<unsigned char, SHA256_DIGEST_LENGTH> hash;

        SHA256_CTX sha256;
        SHA256_Init(&sha256);

        std::ifstream infile(path, std::ios::binary);

        constexpr std::size_t BUFSIZE = 32768;
        std::vector<char> buffer(BUFSIZE);

        while (infile)
        {
            infile.read(buffer.data(), BUFSIZE);
            size_t count = infile.gcount();
            if (!count)
                break;
            SHA256_Update(&sha256, buffer.data(), count);
        }

        SHA256_Final(hash.data(), &sha256);

        return ::mamba::hex_string(hash);
    }

    std::string md5sum(const std::string& path)
    {
        std::array<unsigned char, MD5_DIGEST_LENGTH> hash;

        MD5_CTX md5;
        MD5_Init(&md5);

        std::ifstream infile(path, std::ios::binary);

        constexpr std::size_t BUFSIZE = 32768;
        std::vector<char> buffer(BUFSIZE);

        while (infile)
        {
            infile.read(buffer.data(), BUFSIZE);
            size_t count = infile.gcount();
            if (!count)
                break;
            MD5_Update(&md5, buffer.data(), count);
        }

        MD5_Final(hash.data(), &md5);

        return ::mamba::hex_string(hash);
    }

    bool sha256(const std::string& path, const std::string& validation)
    {
        return sha256sum(path) == validation;
    }

    bool md5(const std::string& path, const std::string& validation)
    {
        return md5sum(path) == validation;
    }

    bool file_size(const fs::path& path, std::uintmax_t validation)
    {
        return fs::file_size(path) == validation;
    }

    int sign(const std::string& data, unsigned char* sk, unsigned char* signature)
    {
        unsigned long long message_len = data.size();
        auto message = (const unsigned char*) data.c_str();

        return crypto_sign_detached(signature, NULL, message, message_len, sk);
    }

    int verify(const std::string& data, unsigned char* pk, unsigned char* signature)
    {
        unsigned long long data_len = data.size();
        auto raw_data = (const unsigned char*) data.c_str();

        return crypto_sign_verify_detached(signature, raw_data, data_len, pk);
    }

    int verify(const std::string& data, const std::string& pk, const std::string& signature)
    {
        unsigned char bin_signature[crypto_sign_BYTES];
        int ret1 = sodium_hex2bin(bin_signature,
                                  crypto_sign_BYTES,
                                  signature.c_str(),
                                  crypto_sign_BYTES * 2U,
                                  NULL,
                                  NULL,
                                  NULL);

        unsigned char bin_pk[crypto_sign_PUBLICKEYBYTES];
        int ret2 = sodium_hex2bin(bin_pk,
                                  crypto_sign_PUBLICKEYBYTES,
                                  pk.c_str(),
                                  crypto_sign_PUBLICKEYBYTES * 2U,
                                  NULL,
                                  NULL,
                                  NULL);

        if (ret1 || ret2)
        {
            throw std::runtime_error("Conversions from hex to bin format failed.");
        }

        return verify(data, bin_pk, bin_signature);
    }

    int verify_gpg_hashed_msg(const std::string& data, unsigned char* pk, unsigned char* signature)
    {
        unsigned char msg_bin[crypto_hash_sha256_BYTES];
        sodium_hex2bin(msg_bin,
                       crypto_hash_sha256_BYTES,
                       data.c_str(),
                       crypto_hash_sha256_BYTES * 2U,
                       NULL,
                       NULL,
                       NULL);

        return crypto_sign_verify_detached(signature, msg_bin, crypto_hash_sha256_BYTES, pk);
    }

    int verify_gpg_hashed_msg(const std::string& data,
                              const std::string& pk,
                              const std::string& signature)
    {
        unsigned char bin_signature[crypto_sign_BYTES];
        sodium_hex2bin(bin_signature,
                       crypto_sign_BYTES,
                       signature.c_str(),
                       crypto_sign_BYTES * 2U,
                       NULL,
                       NULL,
                       NULL);

        unsigned char bin_pk[crypto_sign_PUBLICKEYBYTES];
        sodium_hex2bin(bin_pk,
                       crypto_sign_PUBLICKEYBYTES,
                       pk.c_str(),
                       crypto_sign_PUBLICKEYBYTES * 2U,
                       NULL,
                       NULL,
                       NULL);

        return verify_gpg_hashed_msg(data, bin_pk, bin_signature);
    }
}  // namespace validate
