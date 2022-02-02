#include <reproc++/run.hpp>
#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"

extern "C" {
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
}

using namespace mamba;  // NOLINT(build/namespaces)

#ifndef _WIN32
void daemonize()
{
    pid_t pid, sid;
    int fd;

    // already a daemon
    if (getppid() == 1)
        return;

    // fork parent process
    pid = fork();
    if (pid < 0)
        exit(1);

    // exit parent process
    if (pid > 0)
        exit(0);

    // at this point we are executing as the child process
    // create a new SID for the child process
    sid = setsid();
    if (sid < 0)
        exit(1);

    fd = open("/dev/null", O_RDWR, 0);

    std::cout << fmt::format("Kill process with: kill -s TERM {}", getpid()) << std::endl;

    if (fd != -1)
    {
        dup2 (fd, STDIN_FILENO);
        dup2 (fd, STDOUT_FILENO);
        dup2 (fd, STDERR_FILENO);

        if (fd > 2)
        {
            close (fd);
        }
    }
}
#endif

void
set_run_command(CLI::App* subcom)
{
    init_prefix_options(subcom);

    static std::string streams;
    CLI::Option* stream_option = subcom->add_option("-a,--attach", streams, "Attach to stdin, stdout and/or stderr. -a \"\" for disabling stream redirection")->join(',');

    static std::string cwd;
    subcom->add_option("--cwd", cwd, "Current working directory for command to run in. Defaults to cwd");

    static bool detach = false;
    subcom->add_flag("--daemon", detach, "Detach process from terminal");

    static std::vector<std::string> command;
    subcom->prefix_command();

    subcom->callback([subcom, stream_option]() {
        auto& config = Configuration::instance();
        config.load();

        std::vector<std::string> command = subcom->remaining();

        auto [wrapped_command, script_file]
            = prepare_wrapped_call(Context::instance().target_prefix, command);

        LOG_DEBUG << "Running wrapped script " << join(" ", command);

        bool all_streams = stream_option->count() == 0u;
        bool sinkout = !all_streams && streams.find("stdout") == std::string::npos;
        bool sinkerr = !all_streams && streams.find("stderr") == std::string::npos;

        reproc::options opt;
        if (cwd != "")
        {
            opt.working_directory = cwd.c_str();
        }

        opt.redirect.out.type = sinkout ? reproc::redirect::discard : reproc::redirect::parent;
        opt.redirect.err.type = sinkerr ? reproc::redirect::discard : reproc::redirect::parent;
        auto [_, ec] = reproc::run(wrapped_command, opt);

        if (detach)
        {
            std::cout << fmt::format(fmt::fg(fmt::terminal_color::green), "Running wrapped script {} in the background", join(" ", command)) << std::endl;
            daemonize();
        }

        if (ec)
        {
            std::cerr << ec.message() << std::endl;
        }
    });
}
