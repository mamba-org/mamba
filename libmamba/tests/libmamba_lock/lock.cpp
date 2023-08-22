#include <CLI/CLI.hpp>

#include "mamba/core/context.hpp"
#include "mamba/core/mamba_fs.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util.hpp"

#if defined(__APPLE__) || defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

bool
is_locked(const fs::u8path& path)
{
#ifdef _WIN32
    return mamba::LockFile::is_locked(path);
#else
    // From another process than the lockfile one, we can open/close
    // a new file descriptor without risk to clear locks
    int fd = open(path.string().c_str(), O_RDWR | O_CREAT, 0666);
    bool is_locked = mamba::LockFile::is_locked(fd);
    close(fd);
    return is_locked;
#endif
}

int
main(int argc, char** argv)
{
    CLI::App app{};
    fs::u8path path;
    std::size_t timeout = 1;

    CLI::App* lock_com = app.add_subcommand("lock", "Lock a path");
    lock_com->add_option("path", path, "Path to lock");
    lock_com->add_option("-t,--timeout", timeout, "Timeout in seconds");
    lock_com->callback(
        [&]()
        {
            mamba::Context::instance().lock_timeout = timeout;
            try
            {
                auto lock = mamba::LockFile(path);
                if (lock)
                {
                    std::cout << 1;
                }
                else
                {
                    std::cout << 0;
                }
            }
            catch (...)
            {
                std::cout << 0;
            }
        }
    );

    CLI::App* is_locked_com = app.add_subcommand("is-locked", "Check if a path is locked");
    is_locked_com->add_option("path", path, "Path to check");
    is_locked_com->callback([&]() { std::cout << (fs::exists(path) && is_locked(path)); });

    try
    {
        CLI11_PARSE(app, argc, argv);
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL << e.what();
        mamba::set_sig_interrupted();
        return 1;
    }

    return 0;
}
