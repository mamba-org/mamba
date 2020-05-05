#include <iostream>
#include "openssl/sha.h"
#include "openssl/md5.h"
#include "output.hpp"

#include "validate.hpp"

namespace validate
{
    std::string sha256sum(const std::string& path)
    {
        unsigned char hash[SHA256_DIGEST_LENGTH];

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

        SHA256_Final(hash, &sha256);

        std::stringstream out;
        out.fill('0');
        out << std::hex;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            out << std::setw(2) << (int) hash[i];
        }

        return out.str();
    }

    std::string md5sum(const std::string& path)
    {
        unsigned char hash[MD5_DIGEST_LENGTH];

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

        MD5_Final(hash, &md5);

        std::stringstream out;
        out.fill('0');
        out << std::hex;
        for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            out << std::setw(2) << (int) hash[i];
        }

        return out.str();
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
}

