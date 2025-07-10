// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifdef _WIN32
#include <algorithm>
#endif
#include <filesystem>
#include <string>

#ifndef _WIN32
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
// We can use the presence of UTIME_OMIT to detect platforms that provide
// utimensat.
#if defined(UTIME_OMIT)
#define USE_UTIMENSAT
#endif
#endif

#include "mamba/fs/filesystem.hpp"
#include "mamba/util/encoding.hpp"

namespace mamba::fs
{

#if defined(_WIN32)
    // Maintain `\` on Windows, `/` on other platforms
    std::filesystem::path normalized_separators(std::filesystem::path path)
    {
        auto native_string = path.native();
        static constexpr wchar_t platform_sep = L'\\';
        static constexpr wchar_t other_sep = L'/';
        std::replace(native_string.begin(), native_string.end(), other_sep, platform_sep);
        path = std::move(native_string);
        return path;
    }
#else
    // noop on non-Windows platforms
    std::filesystem::path normalized_separators(std::filesystem::path path)
    {
        return path;
    }
#endif

    std::string to_utf8(const std::filesystem::path& path, Utf8Options utf8_options)
    {
        const auto u8str = [&]
        {
            if (utf8_options.normalize_sep)
            {
                return normalized_separators(path).u8string();
            }
            else
            {
                return path.u8string();
            }
        }();

        return util::to_utf8_std_string(u8str);
    }

    std::filesystem::path from_utf8(std::string_view u8string)
    {
        return normalized_separators(util::to_u8string(u8string));
    }

    void last_write_time(const u8path& path, now, std::error_code& ec) noexcept
    {
#if defined(USE_UTIMENSAT)
        if (utimensat(AT_FDCWD, path.string().c_str(), NULL, 0) == -1)
        {
            ec = std::error_code(errno, std::generic_category());
        }
#else
        auto new_time = fs::file_time_type::clock::now();
        std::filesystem::last_write_time(path, new_time, ec);
#endif
    }


    bool has_modification_rights_on(const u8path& path) {

#if defined(_WIN32) // All Windows platforms
        return true;

#else // UNIX-like platforms

        using std::filesystem::perms;

        // Get permissions
        auto perm = std::filesystem::status(path).permissions();

        // Everybody can modify
        if (perms::none != (perm & perms::others_write))
            return true;

        // Check user and/or group
        if (perms::none != (perm & (perms::owner_write | perms::group_write))) {

            // Get path status
            struct stat info;
            if (stat(path.string().c_str(), &info))
                throw std::runtime_error("Unable to get status information for"
                                         " path " + path.string() + ": "
                                         + strerror(errno));

            // Check user rights
            if (perms::none != (perm & perms::owner_write)
                && info.st_uid == geteuid())
                return true;

            // Check group rights
            if (perms::none != (perm & perms::group_write)) {

                // Check main group
                if (info.st_gid == getegid())
                    return true;

                // Get user name
                auto pwd = getpwuid(geteuid());
                if (! pwd)
                    throw std::runtime_error("Unable to get user name from user"
                                             " ID "
                                             + std::to_string(info.st_uid)
                                             + ": " + strerror(errno));

                // Get first group IDs
                std::vector<gid_t> groups(16);
                int ngroups = groups.size();
                int n = getgrouplist(pwd->pw_name, info.st_gid,
                                     groups.data(), &ngroups);
                if (n < 0) {
                    groups.resize(static_cast<size_t>(n));
                    static_cast<void>(getgrouplist(pwd->pw_name, info.st_gid,
                                                   groups.data(), &ngroups));
                }
                else
                    groups.resize(static_cast<size_t>(ngroups));

                // Find if file group is in list
                auto it = std::find(groups.begin(), groups.end(), info.st_uid);
                if (it != groups.end()) 
                    return true;
          }
        }
        return false;
#endif
    }
}
