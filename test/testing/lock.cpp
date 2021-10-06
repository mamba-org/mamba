#include "mamba/core/context.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/util.hpp"
#include "mamba/core/thread_utils.hpp"

#include <CLI/CLI.hpp>

#if defined(__APPLE__) || defined(__linux__)
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <csignal>
#include <sys/file.h>
#endif


bool
is_locked(const fs::path& path)
{
#ifdef _WIN32
    return mamba::Lock::is_locked(path);
#else
    // From another process than the lockfile one, we can open/close
    // a new file descriptor without risk to clear locks
    int fd = open(path.c_str(), O_RDWR | O_CREAT, 0666);
    bool is_locked = mamba::Lock::is_locked(fd);
    close(fd);
    return is_locked;
#endif
}

int
main(int argc, char** argv)
{
    CLI::App app{};
    fs::path path;
    int timeout = 1;

    CLI::App* lock_com = app.add_subcommand("lock", "Lock a path");
    lock_com->add_option("path", path, "Path to lock");
    lock_com->add_option("-t,--timeout", timeout, "Timeout in seconds");
    lock_com->callback([&]() {
        mamba::Context::instance().lock_timeout = timeout;
        try
        {
            mamba::Lock lock(path);
            std::cout << 1;
        }
        catch (std::runtime_error&)
        {
            std::cout << 0;
        }
    });

    CLI::App* is_locked_com = app.add_subcommand("is-locked", "Check if a path is locked");
    is_locked_com->add_option("path", path, "Path to check");
    is_locked_com->callback([&]() { std::cout << (fs::exists(path) && is_locked(path)); });

    try
    {
        CLI11_PARSE(app, argc, argv);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR << e.what();
        mamba::set_sig_interrupted();
        return 1;
    }

    return 0;
}
