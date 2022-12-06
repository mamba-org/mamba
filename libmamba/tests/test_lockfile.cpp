#include <gtest/gtest.h>

#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/context.hpp"

#include <reproc++/run.hpp>
#include "spdlog/spdlog.h"

#include <string>

#ifdef _WIN32
#include <io.h>

extern "C"
{
#include <io.h>
#include <process.h>
#include <fcntl.h>
}
#endif


namespace mamba
{

#ifndef MAMBA_TEST_LOCK_EXE
#error "MAMBA_TEST_LOCK_EXE must be defined pointing to testing_libmamba_lock"
#endif
    inline static const fs::u8path testing_libmamba_lock_exe = MAMBA_TEST_LOCK_EXE;

    namespace testing
    {

        class LockDirTest : public ::testing::Test
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
                spdlog::set_level(spdlog::level::info);
            }
            void TearDown() override
            {
                mamba::Context::instance().use_lockfiles = true;
            }
        };


        TEST_F(LockDirTest, basics)
        {
            mamba::LockFile lock{ tempdir_path };
            EXPECT_TRUE(lock);
            {
                auto new_lock = std::move(lock);
                EXPECT_FALSE(lock);
                EXPECT_TRUE(new_lock);
            }
            EXPECT_FALSE(lock);
        }


        TEST_F(LockDirTest, disable_locking)
        {
            {
                auto _ = on_scope_exit([] { mamba::Context::instance().use_lockfiles = true; });
                mamba::Context::instance().use_lockfiles = false;
                auto lock = LockFile(tempdir_path);
                EXPECT_FALSE(lock);
            }
            ASSERT_TRUE(mamba::Context::instance().use_lockfiles);
            {
                ASSERT_TRUE(mamba::Context::instance().use_lockfiles);
                auto lock = LockFile(tempdir_path);
                EXPECT_TRUE(lock);
            }
        }

        TEST_F(LockDirTest, same_pid)
        {
            {
                auto lock = LockFile(tempdir_path);
                EXPECT_TRUE(lock.is_locked());
                EXPECT_EQ(lock.count_lock_owners(), 1);
                EXPECT_TRUE(fs::exists(lock.lockfile_path()));

                {
                    auto other_lock = LockFile(tempdir_path);
                    EXPECT_TRUE(other_lock.is_locked());
                    EXPECT_EQ(other_lock.count_lock_owners(), 2);
                    EXPECT_EQ(lock.count_lock_owners(), 2);
                }

                EXPECT_EQ(lock.count_lock_owners(), 1);

                // check the first lock is still locked
                EXPECT_TRUE(fs::exists(lock.lockfile_path()));
            }
            EXPECT_FALSE(fs::exists(tempdir_path / (tempdir_path.filename().string() + ".lock")));

            // we can still re-lock afterwards
            {
                auto lock = LockFile(tempdir_path);
                EXPECT_TRUE(fs::exists(lock.lockfile_path()));
            }
        }

        TEST_F(LockDirTest, different_pid)
        {
            std::string const lock_exe = testing_libmamba_lock_exe.string();
            std::string out, err;
            std::vector<std::string> args;

            {
                auto lock = LockFile(tempdir_path);
                EXPECT_TRUE(fs::exists(lock.lockfile_path()));

                // Check lock status
                EXPECT_TRUE(mamba::LockFile::is_locked(lock));

                // Check lock status from another process
                args = { lock_exe, "is-locked", lock.lockfile_path().string() };
                out.clear();
                err.clear();
                reproc::run(
                    args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

                int is_locked = 0;
                try
                {
                    is_locked = std::stoi(out);
                }
                catch (...)
                {
                    std::cout << "convertion error" << std::endl;
                }
                EXPECT_TRUE(is_locked);

                // Try to lock from another process
                args = { lock_exe, "lock", "--timeout=1", tempdir_path.string() };
                out.clear();
                err.clear();
                reproc::run(
                    args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

                bool new_lock_created = true;
                try
                {
                    new_lock_created = std::stoi(out);
                }
                catch (...)
                {
                    std::cout << "convertion error" << std::endl;
                }
                EXPECT_FALSE(new_lock_created);
            }

            fs::u8path lock_path = tempdir_path / (tempdir_path.filename().string() + ".lock");
            EXPECT_FALSE(fs::exists(lock_path));

            args = { lock_exe, "is-locked", lock_path.string() };
            out.clear();
            err.clear();
            reproc::run(
                args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

            int is_locked = 0;
            try
            {
                is_locked = std::stoi(out);
            }
            catch (...)
            {
            }
            EXPECT_FALSE(is_locked);
        }

        class LockFileTest : public ::testing::Test
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

        TEST_F(LockFileTest, same_pid)
        {
            {
                LockFile lock{ tempfile_path };
                EXPECT_TRUE(lock.is_locked());
                EXPECT_TRUE(fs::exists(lock.lockfile_path()));
                EXPECT_EQ(lock.count_lock_owners(), 1);

                {
                    LockFile other_lock{ tempfile_path };
                    EXPECT_TRUE(other_lock.is_locked());
                    EXPECT_EQ(other_lock.count_lock_owners(), 2);
                    EXPECT_EQ(lock.count_lock_owners(), 2);
                }

                EXPECT_EQ(lock.count_lock_owners(), 1);

                // check the first lock is still locked
                EXPECT_TRUE(fs::exists(lock.lockfile_path()));
            }
            EXPECT_FALSE(fs::exists(tempfile_path.string() + ".lock"));
        }

        TEST_F(LockFileTest, different_pid)
        {
            std::string const lock_exe = testing_libmamba_lock_exe.string();
            std::string out, err;
            std::vector<std::string> args;
            {
                // Create a lock
                auto lock = LockFile(tempfile_path);
                EXPECT_TRUE(fs::exists(lock.lockfile_path()));

                // Check lock status from current PID
                EXPECT_TRUE(mamba::LockFile::is_locked(lock));

                // Check lock status from another process
                args = { lock_exe, "is-locked", lock.lockfile_path().string() };
                out.clear();
                err.clear();
                reproc::run(
                    args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

                int is_locked = 0;
                try
                {
                    is_locked = std::stoi(out);
                }
                catch (...)
                {
                    std::cout << "convertion error" << std::endl;
                }
                EXPECT_TRUE(is_locked);

                // Try to lock from another process
                args = { lock_exe, "lock", "--timeout=1", tempfile_path.string() };
                out.clear();
                err.clear();
                reproc::run(
                    args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

                bool new_lock_created = true;
                try
                {
                    new_lock_created = std::stoi(out);
                }
                catch (...)
                {
                    std::cout << "convertion error" << std::endl;
                }
                EXPECT_FALSE(new_lock_created);
            }

            fs::u8path lock_path = tempfile_path.string() + ".lock";
            EXPECT_FALSE(fs::exists(lock_path));

            args = { lock_exe, "is-locked", lock_path.string() };
            out.clear();
            err.clear();
            reproc::run(
                args, reproc::options{}, reproc::sink::string(out), reproc::sink::string(err));

            int is_locked = 0;
            try
            {
                is_locked = std::stoi(out);
            }
            catch (...)
            {
            }
            EXPECT_FALSE(is_locked);
        }
    }
}
