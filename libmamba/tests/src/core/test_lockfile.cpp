// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <iostream>
#include <string>

#include <catch2/catch_all.hpp>
#include <reproc++/run.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/util.hpp"
#include "mamba/fs/filesystem.hpp"

#include "spdlog/spdlog.h"

#ifdef _WIN32
#include <io.h>

extern "C"
{
#include <fcntl.h>
#include <io.h>
#include <process.h>
}
#endif

#include "mambatests.hpp"

namespace mamba
{
    namespace testing
    {

        class LockDirTest
        {
        protected:

            std::unique_ptr<TemporaryDirectory> p_tempdir;
            fs::u8path tempdir_path;

            LockDirTest()
            {
                p_tempdir = std::make_unique<TemporaryDirectory>();
                tempdir_path = p_tempdir->path();

                spdlog::set_level(spdlog::level::trace);
            }

            ~LockDirTest()
            {
                mamba::allow_file_locking(true);
                spdlog::set_level(spdlog::level::info);
            }
        };

        namespace
        {
            TEST_CASE_FIXTURE(LockDirTest, "basics")
            {
                mamba::LockFile lock{ tempdir_path };
                REQUIRE(lock);
                {
                    auto new_lock = std::move(lock);
                    REQUIRE_FALSE(lock);
                    REQUIRE(new_lock);
                }
                REQUIRE_FALSE(lock);
            }

            TEST_CASE_FIXTURE(LockDirTest, "disable_locking")
            {
                {
                    auto _ = on_scope_exit([] { mamba::allow_file_locking(true); });
                    mamba::allow_file_locking(false);
                    auto lock = LockFile(tempdir_path);
                    REQUIRE_FALSE(lock);
                }
                REQUIRE(mamba::is_file_locking_allowed());
                {
                    REQUIRE(mamba::is_file_locking_allowed());
                    auto lock = LockFile(tempdir_path);
                    REQUIRE(lock);
                }
            }

            TEST_CASE_FIXTURE(LockDirTest, "same_pid")
            {
                {
                    auto lock = LockFile(tempdir_path);
                    REQUIRE(lock.is_locked());
                    CHECK_EQ(lock.count_lock_owners(), 1);
                    REQUIRE(fs::exists(lock.lockfile_path()));

                    {
                        auto other_lock = LockFile(tempdir_path);
                        REQUIRE(other_lock.is_locked());
                        CHECK_EQ(other_lock.count_lock_owners(), 2);
                        CHECK_EQ(lock.count_lock_owners(), 2);
                    }

                    CHECK_EQ(lock.count_lock_owners(), 1);

                    // check the first lock is still locked
                    REQUIRE(fs::exists(lock.lockfile_path()));
                }
                REQUIRE_FALSE(fs::exists(tempdir_path / (tempdir_path.filename().string() + ".lock")));

                // we can still re-lock afterwards
                {
                    auto lock = LockFile(tempdir_path);
                    REQUIRE(fs::exists(lock.lockfile_path()));
                }
            }

            TEST_CASE_FIXTURE(LockDirTest, "different_pid")
            {
                const std::string lock_exe = mambatests::testing_libmamba_lock_exe.string();
                std::string out, err;
                std::vector<std::string> args;

                {
                    auto lock = LockFile(tempdir_path);
                    REQUIRE(fs::exists(lock.lockfile_path()));

                    // Check lock status
                    REQUIRE(mamba::LockFile::is_locked(lock));

                    // Check lock status from another process
                    args = { lock_exe, "is-locked", lock.lockfile_path().string() };
                    out.clear();
                    err.clear();
                    reproc::run(
                        args,
                        reproc::options{},
                        reproc::sink::string(out),
                        reproc::sink::string(err)
                    );

                    int is_locked = 0;
                    try
                    {
                        is_locked = std::stoi(out);
                    }
                    catch (...)
                    {
                        std::cout << "conversion error" << std::endl;
                    }
                    REQUIRE(is_locked);

                    // Try to lock from another process
                    args = { lock_exe, "lock", "--timeout=1", tempdir_path.string() };
                    out.clear();
                    err.clear();
                    reproc::run(
                        args,
                        reproc::options{},
                        reproc::sink::string(out),
                        reproc::sink::string(err)
                    );

                    bool new_lock_created = true;
                    try
                    {
                        new_lock_created = std::stoi(out);
                    }
                    catch (...)
                    {
                        std::cout << "conversion error" << std::endl;
                    }
                    REQUIRE_FALSE(new_lock_created);
                }

                fs::u8path lock_path = tempdir_path / (tempdir_path.filename().string() + ".lock");
                REQUIRE_FALSE(fs::exists(lock_path));

                args = { lock_exe, "is-locked", lock_path.string() };
                out.clear();
                err.clear();
                reproc::run(args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

                int is_locked = 0;
                try
                {
                    is_locked = std::stoi(out);
                }
                catch (...)
                {
                }
                REQUIRE_FALSE(is_locked);
            }
        }

        class LockFileTest
        {
        protected:

            std::unique_ptr<TemporaryFile> p_tempfile;
            fs::u8path tempfile_path;

            LockFileTest()
            {
                p_tempfile = std::make_unique<TemporaryFile>();
                tempfile_path = p_tempfile->path();

                spdlog::set_level(spdlog::level::trace);
            }

            ~LockFileTest()
            {
                spdlog::set_level(spdlog::level::info);
            }
        };

        namespace
        {
            TEST_CASE_FIXTURE(LockFileTest, "same_pid")
            {
                {
                    LockFile lock{ tempfile_path };
                    REQUIRE(lock.is_locked());
                    REQUIRE(fs::exists(lock.lockfile_path()));
                    CHECK_EQ(lock.count_lock_owners(), 1);

                    {
                        LockFile other_lock{ tempfile_path };
                        REQUIRE(other_lock.is_locked());
                        CHECK_EQ(other_lock.count_lock_owners(), 2);
                        CHECK_EQ(lock.count_lock_owners(), 2);
                    }

                    CHECK_EQ(lock.count_lock_owners(), 1);

                    // check the first lock is still locked
                    REQUIRE(fs::exists(lock.lockfile_path()));
                }
                REQUIRE_FALSE(fs::exists(tempfile_path.string() + ".lock"));
            }

            TEST_CASE_FIXTURE(LockFileTest, "different_pid")
            {
                const std::string lock_exe = mambatests::testing_libmamba_lock_exe.string();
                std::string out, err;
                std::vector<std::string> args;
                {
                    // Create a lock
                    auto lock = LockFile(tempfile_path);
                    REQUIRE(fs::exists(lock.lockfile_path()));

                    // Check lock status from current PID
                    REQUIRE(mamba::LockFile::is_locked(lock));

                    // Check lock status from another process
                    args = { lock_exe, "is-locked", lock.lockfile_path().string() };
                    out.clear();
                    err.clear();
                    reproc::run(
                        args,
                        reproc::options{},
                        reproc::sink::string(out),
                        reproc::sink::string(err)
                    );

                    int is_locked = 0;
                    try
                    {
                        is_locked = std::stoi(out);
                    }
                    catch (...)
                    {
                        std::cout << "conversion error" << std::endl;
                    }
                    REQUIRE(is_locked);

                    // Try to lock from another process
                    args = { lock_exe, "lock", "--timeout=1", tempfile_path.string() };
                    out.clear();
                    err.clear();
                    reproc::run(
                        args,
                        reproc::options{},
                        reproc::sink::string(out),
                        reproc::sink::string(err)
                    );

                    bool new_lock_created = true;
                    try
                    {
                        new_lock_created = std::stoi(out);
                    }
                    catch (...)
                    {
                        std::cout << "conversion error" << std::endl;
                    }
                    REQUIRE_FALSE(new_lock_created);
                }

                fs::u8path lock_path = tempfile_path.string() + ".lock";
                REQUIRE_FALSE(fs::exists(lock_path));

                args = { lock_exe, "is-locked", lock_path.string() };
                out.clear();
                err.clear();
                reproc::run(args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

                int is_locked = 0;
                try
                {
                    is_locked = std::stoi(out);
                }
                catch (...)
                {
                }
                REQUIRE_FALSE(is_locked);
            }
        }
    }
}
