#ifndef MAMBA_PACKAGE_CACHE
#define MAMBA_PACKAGE_CACHE

#include <vector>
#include <string>
#include <iostream>

#include "output.hpp"
#include "context.hpp"
#include "fsutil.hpp"
#include "environment.hpp"

#define PACKAGE_CACHE_MAGIC_FILE "urls.txt"

namespace mamba
{
    // TODO layered package caches
    class PackageCacheData
    {
    public:

        enum Writable
        {
            UNKNOWN,
            WRITABLE,
            NOT_WRITABLE,
            DIR_DOES_NOT_EXIST
        };

        PackageCacheData(const fs::path& pkgs_dir);

        bool create_directory();
        void set_writable(Writable writable);
        Writable is_writable();

        static PackageCacheData first_writable(const std::vector<fs::path>* pkgs_dirs = nullptr);

    private:

        void check_writable();

        Writable m_writable = UNKNOWN;
        fs::path m_pkgs_dir;
    };
}

#endif