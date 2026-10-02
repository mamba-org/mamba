// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <regex>
#include <utility>

#include <openssl/evp.h>

#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/cryptography.hpp"
#include "mamba/validation/tools.hpp"

namespace mamba::validation
{
    auto sha256sum(const fs::u8path& path) -> std::string
    {
        thread_local auto hasher = util::Sha256Hasher();

        std::ifstream infile = mamba::open_ifstream(path);
        return hasher.file_hex_str(infile);
    }

    auto md5sum(const fs::u8path& path) -> std::string
    {
        thread_local auto hasher = util::Md5Hasher();

        std::ifstream infile = mamba::open_ifstream(path);
        return hasher.file_hex_str(infile);
    }

    auto file_size(const fs::u8path& path, std::uintmax_t validation) -> bool
    {
        return fs::file_size(path) == validation;
    }
}
