// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_FS_UTIL
#define MAMBA_CORE_FS_UTIL

#include "environment.hpp"
#include "mamba_fs.hpp"
#include "util.hpp"

#include <iostream>
#include <string>
#include <system_error>

namespace mamba
{
    namespace path
    {
        inline bool starts_with_home(const fs::path& p)
        {
            std::string path = p;
            return path[0] == '~'
                   || starts_with(env::expand_user(path).string(), env::expand_user("~").string());
        }

        // TODO more error handling
        inline void create_directories_sudo_safe(const fs::path& path)
        {
            if (fs::is_directory(path))
                return;

            fs::path base_dir = path.parent_path();
            if (!fs::is_directory(base_dir))
            {
                create_directories_sudo_safe(base_dir);
            }
            fs::create_directory(path);

#ifndef _WIN32
            // set permissions to 0o2775
            fs::permissions(path,
                            fs::perms::set_gid | fs::perms::owner_all | fs::perms::group_all
                                | fs::perms::others_read | fs::perms::others_exec);
#endif
        }

        inline bool touch(fs::path path, bool mkdir = false, bool sudo_safe = false)
        {
            // TODO error handling!
            path = env::expand_user(path);
            if (lexists(path))
            {
                fs::last_write_time(path, fs::file_time_type::clock::now());
                return true;
            }
            else
            {
                auto dirpath = path.parent_path();
                if (!fs::is_directory(dirpath) && mkdir)
                {
                    if (sudo_safe)
                    {
                        create_directories_sudo_safe(dirpath);
                    }
                    else
                    {
                        fs::create_directories(dirpath);
                    }
                }
                // directory exists, now create empty file
                std::ofstream ostream = open_ofstream(path, std::ios::out);
                if (ostream.fail())
                    throw fs::filesystem_error("File creation failed",
                                               std::make_error_code(std::errc::permission_denied));

                return false;
            }
        }

        inline bool is_writable(const fs::path& path)
        {
            if (fs::is_directory(path.parent_path()))
            {
                bool path_existed = lexists(path);
                std::ofstream test = open_ofstream(path, std::ios_base::out | std::ios_base::app);
                bool is_writable = test.is_open();
                if (!path_existed)
                {
                    test.close();
                    fs::remove(path);
                }
                return is_writable;
            }
            else
            {
                throw std::runtime_error("Cannot check file path at " + path.string()
                                         + " for accessibility.");
            }
        }
    }  // namespace path
}  // namespace mamba

#endif
