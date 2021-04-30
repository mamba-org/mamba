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

#ifdef BUILD_CRYPTO_PACKAGE_VALIDATION
    int sign(const std::string& data, unsigned char* sk, unsigned char* signature);

    int verify(const std::string& data, unsigned char* pk, unsigned char* signature);
    int verify(const std::string& data, const std::string& pk, const std::string& signature);

    int verify_gpg_hashed_msg(const std::string& data, unsigned char* pk, unsigned char* signature);
    int verify_gpg_hashed_msg(const std::string& data,
                              const std::string& pk,
                              const std::string& signature);
#endif

}  // namespace validate

#endif  // MAMBA_VALIDATE_HPP
