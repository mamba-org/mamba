#include <csignal>
#include <exception>

#include <reproc++/run.hpp>
#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/util_os.hpp"

#include <spdlog/fmt/ostr.h>
#include <nlohmann/json.hpp>

#ifndef _WIN32
extern "C" {
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
}
#endif


namespace mamba {

    std::string generate_unique_process_name(std::string_view process_name)
    {
        assert(!process_name.empty());

        static constexpr std::array prefixes = {
            "curious",
            "gentle",
            "happy",
            "stubborn",
            // TODO: add more here
        };

        std::string selected_prefix;
        std::sample(prefixes.begin(), prefixes.end(), &selected_prefix, 1, local_random_generator());
        const auto new_process_name = fmt::format("{}_{}", selected_prefix, process_name);
        return new_process_name;
    }

    const fs::path& proc_dir()
    {
        static auto path = env::home_directory() / ".mamba" / "proc";
        return path;
    }

    std::unique_ptr<LockFile> lock_proc_dir()
    {
        fs::create_directories(proc_dir());
        auto lockfile = LockFile::try_lock(proc_dir());
        if(!lockfile)
        {
            throw std::runtime_error(fmt::format("'mamba run' failed to lock ({}) or lockfile was not properly deleted", proc_dir()));
        }
        return lockfile;
    }

    class ScopedProcFile
    {
        const fs::path location;

    public:

        ScopedProcFile(const std::string& child_name, PID child_pid)
            : location{ proc_dir() / fmt::format("{}.json", getpid()) }
        {
            const auto lock = lock_proc_dir();
            const auto open_mode = std::ios::binary | std::ios::trunc | std::ios::in | std::ios::out;
            fs::fstream pid_file{ location, open_mode };
            if(!pid_file.is_open())
            {
                throw std::runtime_error(fmt::format("'mamba run' failed to open/create file: {}", location));
            }

            nlohmann::json file_json;
            auto& process_info = file_json["running"][child_name];
            process_info["pid"] = child_pid;
            // TODO: add other info here if necessary
            pid_file << file_json;
        }

        ~ScopedProcFile()
        {
            const auto lock = lock_proc_dir();
            std::error_code errcode;
            const bool is_removed = fs::remove(location, errcode);
            if(!is_removed)
            {
                LOG_WARNING << fmt::format("Failed to remove file '{}' : {}", location, errcode.message());
            }
        }
    };

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

    std::cout << fmt::format("Kill process with: kill {}", getpid()) << std::endl;

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
#ifndef _WIN32
    subcom->add_flag("-d,--detach", detach, "Detach process from terminal");
#endif

    static bool clean_env = false;
    subcom->add_flag("--clean-env", clean_env, "Start with a clean environment");

    static std::vector<std::string> env_vars;
    subcom->add_option("-e,--env", env_vars, "Add env vars with -e ENVVAR or -e ENVVAR=VALUE")->allow_extra_args(false);

    subcom->prefix_command();

    static reproc::process proc;

    subcom->callback([subcom, stream_option]() {
    try {
        auto& config = Configuration::instance();
        config.at("show_banner").set_value(false);
        config.load();

        std::vector<std::string> command = subcom->remaining();
        if (command.empty())
        {
            LOG_ERROR << "Did not receive any command to run inside environment";
            exit(1);
        }

        LOG_WARNING << "Remaining args to run as command: " << join(" ", command); // TODO: lower log level and then check why they never print in info and debug

        // replace the wrapping bash with new process entirely
        #ifndef _WIN32
        if (command.front() != "exec")
            command.insert(command.begin(), "exec");

        const std::string process_name = [&]{
            // Insert a unique process name associated to the command.
            command.reserve(4); // We need at least 4 objects to not move around.
            const auto original_name_it = std::next(command.begin());
            const auto& original_name = *original_name_it;
            auto unique_name = generate_unique_process_name(original_name);
            command.insert(original_name_it, { {"-a"}, std::move(unique_name) });
            return unique_name;
        }();
        #endif

        auto [wrapped_command, script_file]
            = prepare_wrapped_call(Context::instance().target_prefix, command);

        LOG_WARNING << "Running wrapped script: " << join(" ", command); // TODO: lower log level and then check why they never print in info and debug

        bool all_streams = stream_option->count() == 0u;
        bool sinkout = !all_streams && streams.find("stdout") == std::string::npos;
        bool sinkerr = !all_streams && streams.find("stderr") == std::string::npos;
        bool sinkin = !all_streams && streams.find("stdin") == std::string::npos;

        reproc::options opt;
        if (cwd != "")
        {
            opt.working_directory = cwd.c_str();
        }

        if (clean_env)
        {
            opt.env.behavior = reproc::env::empty;
        }

        std::map<std::string, std::string> env_map;
        if (env_vars.size())
        {
            for (auto& e : env_vars)
            {
                if (e.find_first_of("=") != std::string::npos)
                {
                    auto split_e = split(e, "=", 1);
                    env_map[split_e[0]] = split_e[1];
                }
                else
                {
                    auto val = env::get(e);
                    if (val)
                    {
                        env_map[e] = val.value();
                    }
                    else
                    {
                        LOG_WARNING << "Requested env var " << e  << " does not exist in environment";
                    }
                }
            }
            opt.env.extra = env_map;
        }

        opt.redirect.out.type = sinkout ? reproc::redirect::discard : reproc::redirect::parent;
        opt.redirect.err.type = sinkerr ? reproc::redirect::discard : reproc::redirect::parent;
        opt.redirect.in.type = sinkin ? reproc::redirect::discard : reproc::redirect::parent;

#ifndef _WIN32
        if (detach)
        {
            std::cout << fmt::format(fmt::fg(fmt::terminal_color::green), "Running wrapped script {} in the background", join(" ", command)) << std::endl;
            daemonize();
        }
#endif

        int status;
        PID pid;
        std::error_code ec;

        ec = proc.start(wrapped_command, opt);

        std::tie(pid, ec) = proc.pid();

        static ScopedProcFile scoped_proc_file{ process_name, pid }; // Note: this is static only so that calls to `std::exit()` will call the destructor.

        if (ec)
        {
            std::cerr << ec.message() << std::endl;
            exit(1);
        }

#ifndef _WIN32
        std::thread t([](){
            signal(SIGTERM, [](int signum) {
                LOG_INFO << "Received SIGTERM on micromamba run - terminating process";
                reproc::stop_actions sa;
                sa.first = reproc::stop_action{reproc::stop::terminate, std::chrono::milliseconds(3000)};
                sa.second = reproc::stop_action{reproc::stop::kill, std::chrono::milliseconds(3000)};
                proc.stop(sa);
            });
        });
        t.detach();
#endif

        // check if we need this
        if (!opt.redirect.discard && opt.redirect.file == nullptr &&
            opt.redirect.path == nullptr) {
          opt.redirect.parent = true;
        }

        ec = reproc::drain(proc, reproc::sink::null, reproc::sink::null);

        std::tie(status, ec) = proc.stop(opt.stop);

        if (ec)
        {
            std::cerr << ec.message() << std::endl;
        }

        // exit with status code from reproc
        exit(status);
    }
    catch(const std::runtime_error& ex)
    {
        LOG_ERROR << "run command failed:" << ex.what();
        exit(1);
    }
    });
}
