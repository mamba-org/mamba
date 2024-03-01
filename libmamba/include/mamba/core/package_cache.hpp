// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_PACKAGE_CACHE
#define MAMBA_CORE_PACKAGE_CACHE

#include <map>
#include <string>
#include <vector>

#include "mamba/core/fsutil.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/specs/package_info.hpp"

#define PACKAGE_CACHE_MAGIC_FILE "urls.txt"

namespace mamba
{
    struct ValidationParams;

    enum class Writable
    {
        UNKNOWN,
        WRITABLE,
        NOT_WRITABLE,
        DIR_DOES_NOT_EXIST
    };

    // TODO layered package caches
    class PackageCacheData
    {
    public:

        explicit PackageCacheData(const fs::u8path& path);

        bool create_directory();
        void set_writable(Writable writable);
        Writable is_writable();
        fs::u8path path() const;
        void clear_query_cache(const specs::PackageInfo& s);

        bool has_valid_tarball(const specs::PackageInfo& s, const ValidationParams& params);
        bool has_valid_extracted_dir(const specs::PackageInfo& s, const ValidationParams& params);

    private:

        void check_writable();

        std::map<std::string, bool> m_valid_tarballs;
        std::map<std::string, bool> m_valid_extracted_dir;
        Writable m_writable = Writable::UNKNOWN;
        fs::u8path m_path;
    };

    class MultiPackageCache
    {
    public:

        MultiPackageCache(const std::vector<fs::u8path>& pkgs_dirs, const ValidationParams& params);

        std::vector<fs::u8path> paths() const;

        fs::u8path get_tarball_path(const specs::PackageInfo& s, bool return_empty = true);
        fs::u8path get_extracted_dir_path(const specs::PackageInfo& s, bool return_empty = true);

        fs::u8path first_writable_path();
        PackageCacheData& first_writable_cache(bool create = false);
        std::vector<PackageCacheData*> writable_caches();

        void clear_query_cache(const specs::PackageInfo& s);

    private:

        std::vector<PackageCacheData> m_caches;
        std::map<std::string, fs::u8path> m_cached_tarballs;
        std::map<std::string, fs::u8path> m_cached_extracted_dirs;
        const ValidationParams& m_params;
    };
}  // namespace mamba

#endif
