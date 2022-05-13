// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef MAMBA_CORE_FS_UTIL
#define MAMBA_CORE_FS_UTIL

#include <string>
#include <system_error>

#include "mamba/core/environment.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/core/output.hpp"

namespace mamba
{
    namespace path
    {
        inline bool starts_with_home(const fs::path& p)
        {
            std::string path = p.string();
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
                std::ofstream outfile;
#if _WIN32
                outfile.open(path.wstring(), std::ios::out);
#else
                outfile.open(path, std::ios::out);
#endif

                if (!outfile.good())
                    LOG_INFO << "Could not touch file at " << path;

                if (outfile.fail())
                    throw fs::filesystem_error("File creation failed",
                                               std::make_error_code(std::errc::permission_denied));

                return false;
            }
        }

        // Returns `true` only if the provided path is either:
        // - a file we are able to open for writing;
        // - a directory we are able to create a file in for writing;
        // - a file name that does not exist but the parent directory in the path exists and we
        //   are able to create a file with that name in that directory for writing.
        // Returns `false` otherwise.
        inline bool is_writable(const fs::path& path) noexcept
        {
            const auto& path_to_write_in = fs::exists(path) ? path : path.parent_path();

            static constexpr auto writable_flags
                = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
            std::error_code ec;
            const auto status = fs::status(path_to_write_in, ec);

            const bool should_be_writable
                = !ec && status.type() != fs::file_type::not_found
                  && (status.permissions() & writable_flags) != fs::perms::none;

            // If it should not be writable, stop there.
            if (!should_be_writable)
                return false;

            // If it should be, check that it's true by creating or editing a file.
            const bool is_directory
                = fs::exists(path)
                  && fs::is_directory(path, ec);  // fs::is_directory fails if path does not exist
            if (ec)
                return false;

            const auto test_file_path
                = is_directory ? path / ".mamba-is-writable-check-delete-me" : path;
            const auto _ = on_scope_exit(
                [&]
                {
                    if (is_directory)
                    {
                        std::error_code ec;
                        fs::remove(test_file_path, ec);
                    }
                });
#if _WIN32
            std::ofstream test_file{ test_file_path.wstring(),
                                     std::ios_base::out | std::ios_base::app };
#else
            std::ofstream test_file{ test_file_path, std::ios_base::out | std::ios_base::app };
#endif
            return test_file.is_open();
        }

    }  // namespace path
}  // namespace mamba

#endif
