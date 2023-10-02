// Copyright (c) 2023, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <fstream>
#include <string>
#include <system_error>

#include "mamba/core/environment.hpp"
#include "mamba/core/fsutil.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/util_scope.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/string.hpp"

namespace mamba::path
{
    bool starts_with_home(const fs::u8path& p)
    {
        std::string path = p.string();
        return path[0] == '~'
               || util::starts_with(env::expand_user(path).string(), env::expand_user("~").string());
    }

    // TODO more error handling
    void create_directories_sudo_safe(const fs::u8path& path)
    {
        if (fs::is_directory(path))
        {
            return;
        }

        fs::u8path base_dir = path.parent_path();
        if (!fs::is_directory(base_dir))
        {
            create_directories_sudo_safe(base_dir);
        }
        fs::create_directory(path);

#ifndef _WIN32
        // set permissions to 0o2775
        fs::permissions(
            path,
            fs::perms::set_gid | fs::perms::owner_all | fs::perms::group_all
                | fs::perms::others_read | fs::perms::others_exec
        );
#endif
    }

    namespace
    {
        bool touch_impl(fs::u8path path, bool mkdir, bool sudo_safe)
        {
            // TODO error handling!
            path = env::expand_user(path);
            if (lexists(path))
            {
                fs::last_write_time(path, fs::now());
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
                std::ofstream outfile{ path.std_path(), std::ios::out };

                if (!outfile.good())
                {
                    LOG_INFO << "Could not touch file at " << path;
                }

                if (outfile.fail())
                {
                    throw fs::filesystem_error(
                        "File creation failed",
                        std::make_error_code(std::errc::permission_denied)
                    );
                }

                return false;
            }
        }
    }

    bool touch(const fs::u8path& path, bool mkdir, bool sudo_safe)
    {
        return touch_impl(path, mkdir, sudo_safe);
    }

    bool is_writable(const fs::u8path& path) noexcept
    {
        const bool path_exists = fs::exists(path);

        const auto& path_to_write_in = path_exists ? path : path.parent_path();

        static constexpr auto writable_flags = fs::perms::owner_write | fs::perms::group_write
                                               | fs::perms::others_write;
        std::error_code ec;
        const auto status = fs::status(path_to_write_in, ec);

        const bool should_be_writable = !ec && status.type() != fs::file_type::not_found
                                        && (status.permissions() & writable_flags) != fs::perms::none;

        // If it should not be writable, stop there.
        if (!should_be_writable)
        {
            return false;
        }

        // If it should be, check that it's true by creating or editing a file.
        const bool is_directory = path_exists && fs::is_directory(path, ec);  // fs::is_directory
                                                                              // fails if
                                                                              // path does
                                                                              // not exist

        if (ec)
        {
            return false;
        }

        const auto& test_file_path = is_directory ? path / ".mamba-is-writable-check-delete-me"
                                                  : path;
        const auto _ = on_scope_exit(
            [&]
            {
                // If it is a directory we created a subitem in that directory and need to delete it
                // If the path didn't exist before, we created a file that needs to be removed
                // as well
                if (is_directory || path_exists == false)
                {
                    std::error_code lec;
                    fs::remove(test_file_path, lec);
                }
            }
        );
        std::ofstream test_file{ test_file_path.std_path(), std::ios_base::out | std::ios_base::app };
        return test_file.is_open();
    }
}  // namespace path

namespace mamba::mamba_fs
{
    void rename_or_move(const fs::u8path& from, const fs::u8path& to, std::error_code& ec)
    {
        fs::rename(from, to, ec);
        if (ec)
        {
            ec.clear();
            fs::copy_file(from, to, ec);
            if (ec)
            {
                std::error_code ec2;
                fs::remove(to, ec2);
            }
        }
    }
}
