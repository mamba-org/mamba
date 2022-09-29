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
    namespace testing
    {
#ifdef _WIN32
#define EXPECT_LOCKED(lock) EXPECT_TRUE(mamba::LockFile::is_locked(lock->lockfile_path()));
#else
#define EXPECT_LOCKED(lock) EXPECT_TRUE(mamba::LockFile::is_locked(lock->fd()));
#endif

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

        TEST_F(LockDirTest, disable_locking)
        {
            {
                mamba::Context::instance().use_lockfiles = false;
                auto lock = LockFile::create_lock(tempdir_path);
                EXPECT_TRUE(lock == nullptr);
            }
        }

        TEST_F(LockDirTest, same_pid)
        {
            {
                auto lock = LockFile::create_lock(tempdir_path);
                EXPECT_TRUE(fs::exists(lock->lockfile_path()));

                EXPECT_THROW(LockFile::create_lock(tempdir_path), std::logic_error);

                // check the first lock is still locked
                EXPECT_TRUE(fs::exists(lock->lockfile_path()));
            }
            EXPECT_FALSE(fs::exists(tempdir_path / (tempdir_path.filename().string() + ".lock")));
        }

        TEST_F(LockDirTest, different_pid)
        {
            std::string lock_cli;
            std::string out, err;
            std::vector<std::string> args;

#ifdef _WIN32
            lock_cli = "testing_libmamba_lock";
#else
            lock_cli = "./testing_libmamba_lock";
#endif

            {
                auto lock = LockFile::create_lock(tempdir_path);
                EXPECT_TRUE(fs::exists(lock->lockfile_path()));

                // Check lock PID
                int pid = getpid();
                EXPECT_EQ(mamba::LockFile::read_pid(lock->fd()), pid);

                // Check lock status
                EXPECT_LOCKED(lock);

                // Check lock status from another process
                args = { lock_cli, "is-locked", lock->lockfile_path().string() };
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
                args = { lock_cli, "lock", "--timeout=1", tempdir_path.string() };
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

                // Check the PID hasn't changed
                EXPECT_EQ(mamba::LockFile::read_pid(lock->fd()), pid);
            }

            fs::u8path lock_path = tempdir_path / (tempdir_path.filename().string() + ".lock");
            EXPECT_FALSE(fs::exists(lock_path));

            args = { lock_cli, "is-locked", lock_path.string() };
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
                auto lock = LockFile::create_lock(tempfile_path);
                EXPECT_TRUE(fs::exists(lock->lockfile_path()));

                EXPECT_THROW(LockFile::create_lock(tempfile_path), std::logic_error);

                // check the first lock is still locked
                EXPECT_TRUE(fs::exists(lock->lockfile_path()));
            }
            EXPECT_FALSE(fs::exists(tempfile_path.string() + ".lock"));
        }

        TEST_F(LockFileTest, different_pid)
        {
            std::string lock_cli;
            std::string out, err;
            std::vector<std::string> args;

#ifdef _WIN32
            lock_cli = "testing_libmamba_lock";
#else
            lock_cli = "./testing_libmamba_lock";
#endif
            {
                // Create a lock
                auto lock = LockFile::create_lock(tempfile_path);
                EXPECT_TRUE(fs::exists(lock->lockfile_path()));

                // Check lock PID
                int pid = getpid();
                EXPECT_EQ(mamba::LockFile::read_pid(lock->fd()), pid);

                // Check lock status from current PID
                EXPECT_LOCKED(lock);

                // Check lock status from another process
                args = { lock_cli, "is-locked", lock->lockfile_path().string() };
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
                args = { lock_cli, "lock", "--timeout=1", tempfile_path.string() };
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

                // Check the PID hasn't changed
                EXPECT_EQ(mamba::LockFile::read_pid(lock->fd()), pid);
            }

            fs::u8path lock_path = tempfile_path.string() + ".lock";
            EXPECT_FALSE(fs::exists(lock_path));

            args = { lock_cli, "is-locked", lock_path.string() };
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
#undef EXPECT_LOCKED
    }
}
