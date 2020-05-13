#ifndef MAMBA_PACKAGE_CACHE
#define MAMBA_PACKAGE_CACHE

#include <vector>
#include <string>
#include <iostream>

#include "output.hpp"
#include "context.hpp"
#include "fsutil.hpp"
#include "environment.hpp"
#include "package_info.hpp"

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

        bool query(const PackageInfo& s);

        static PackageCacheData first_writable(const std::vector<fs::path>* pkgs_dirs = nullptr);

    private:

        void check_writable();

        std::map<std::string, bool> m_valid_cache;
        Writable m_writable = UNKNOWN;
        fs::path m_pkgs_dir;
    };

    class MultiPackageCache
    {
    public:
        MultiPackageCache(const std::vector<fs::path>& pkgs_dirs);
        PackageCacheData& first_writable();

        bool query(const PackageInfo& s);

    private:
        std::vector<PackageCacheData> m_caches;
    };
}

#endif