#ifndef MAMBA_VALIDATE_HPP
#define MAMBA_VALIDATE_HPP

#include "thirdparty/filesystem.hpp"

namespace fs = ghc::filesystem;

namespace validate
{
    std::string sha256sum(const std::string& path, std::size_t filesize);
    std::string md5sum(const std::string& path, std::size_t filesize);
    bool sha256(const std::string& path, std::size_t filesize, const std::string& validation);
    bool md5(const std::string& path, std::size_t filesize, const std::string& validation);
    bool file_size(const fs::path& path, std::uintmax_t validation);
}

#endif //MAMBA_VALIDATE_HPP
