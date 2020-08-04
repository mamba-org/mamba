// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "validate.hpp"

#include <iostream>

#include "openssl/md5.h"
#include "openssl/sha.h"
#include "output.hpp"
#include "util.hpp"

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
}  // namespace validate
