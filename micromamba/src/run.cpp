#include <csignal>
#include <exception>
#include <thread>

#include "spdlog/spdlog.h"
#ifdef SPDLOG_FMT_EXTERNAL
#include "fmt/color.h"
#else
#include "spdlog/fmt/bundled/color.h"
#endif

#include <reproc++/run.hpp>
#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/util_random.hpp"
#include "mamba/core/execution.hpp"

#include <nlohmann/json.hpp>

#ifndef _WIN32
extern "C"
{
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
}
#else
#include <process.h>
#endif

#include "run.hpp"

namespace mamba
{
    std::string generate_unique_process_name(std::string_view program_name)
    {
        assert(!program_name.empty());

        static const std::vector prefixes = {
            "curious",   "gentle",   "happy",      "stubborn",   "boring",  "interesting",
            "funny",     "weird",    "surprising", "serious",    "tender",  "obvious",
            "great",     "proud",    "silent",     "loud",       "vacuous", "focused",
            "pretty",    "slick",    "tedious",    "stubborn",   "daring",  "tenacious",
            "resilient", "rigorous", "friendly",   "creative",   "polite",  "frank",
            "honest",    "warm",     "smart",      "intriguing",
            // TODO: add more here
        };

        static std::vector alt_names{
            "program", "application", "app", "code", "blob", "binary", "script",
        };

        static std::vector prefixes_bag = prefixes;
        std::string selected_name{ program_name };
        while (true)
        {
            std::string selected_prefix;
            if (!prefixes_bag.empty())
            {
                // Pick a random prefix from our bag of prefixes.
                const auto selected_prefix_idx
                    = random_int<std::size_t>(0, prefixes_bag.size() - 1);
                const auto selected_prefix_it
                    = std::next(prefixes_bag.begin(), selected_prefix_idx);
                selected_prefix = *selected_prefix_it;
                prefixes_bag.erase(selected_prefix_it);
            }
            else if (!alt_names.empty())
            {
                // No more prefixes: we retry the same prefixes but with a different program name.
                const auto selected_name_idx = random_int<std::size_t>(0, alt_names.size() - 1);
                const auto selected_name_it = std::next(alt_names.begin(), selected_name_idx);
                selected_name = *selected_name_it;
                alt_names.erase(selected_name_it);
                prefixes_bag = prefixes;  // Re-fill the prefix bag.
                continue;                 // Re-try with new prefix + new name.
            }
            else
            {
                // No prefixes left in the bag nor alternative names, just generate a random prefix
                // as a fail-safe.
                constexpr std::size_t arbitrary_prefix_length = 8;
                selected_prefix = generate_random_alphanumeric_string(arbitrary_prefix_length);
                selected_name = program_name;
            }

            const auto new_process_name = fmt::format("{}_{}", selected_prefix, selected_name);
            if (!is_process_name_running(new_process_name))
                return new_process_name;
        }
    }

    const fs::path& proc_dir()
    {
        static auto path = env::home_directory() / ".mamba" / "proc";
        return path;
    }

    std::unique_ptr<LockFile> lock_proc_dir()
    {
        auto lockfile = LockFile::try_lock(proc_dir());
        if (!lockfile)
        {
            throw std::runtime_error(
                fmt::format("'mamba run' failed to lock ({}) or lockfile was not properly deleted",
                            proc_dir().string()));
        }
        return lockfile;
    }

    nlohmann::json get_all_running_processes_info(
        const std::function<bool(const nlohmann::json&)>& filter)
    {
        nlohmann::json all_processes_info;

        const auto open_mode = std::ios::binary | std::ios::in;

        for (auto&& entry : fs::directory_iterator{ proc_dir() })
        {
            const auto file_location = entry.path();
            if (file_location.extension() != ".json")
                continue;

            std::ifstream pid_file{ file_location, open_mode };
            if (!pid_file.is_open())
            {
                LOG_WARNING << fmt::format("failed to open {}", file_location.string());
                continue;
            }

            auto running_processes_info = nlohmann::json::parse(pid_file);
            running_processes_info["pid"] = file_location.filename().replace_extension();
            if (!filter || filter(running_processes_info))
                all_processes_info.push_back(running_processes_info);
        }

        return all_processes_info;
    }

    bool is_process_name_running(const std::string& name)
    {
        const auto other_processes_with_same_name = get_all_running_processes_info(
            [&](const nlohmann::json& process_info) { return process_info["name"] == name; });
        return !other_processes_with_same_name.empty();
    }


    class ScopedProcFile
    {
        const fs::path location;

    public:
        ScopedProcFile(const std::string& name,
                       const std::vector<std::string>& command,
                       std::unique_ptr<LockFile> proc_dir_lock = lock_proc_dir())
            : location{ proc_dir() / fmt::format("{}.json", getpid()) }
        {
            assert(proc_dir_lock);  // Lock must be hold for the duraction of this constructor.

            const auto open_mode = std::ios::binary | std::ios::trunc | std::ios::out;
            std::ofstream pid_file{ location, open_mode };
            if (!pid_file.is_open())
            {
                throw std::runtime_error(
                    fmt::format("'mamba run' failed to open/create file: {}", location.string()));
            }

            nlohmann::json file_json;
            file_json["name"] = name;
            file_json["command"] = command;
            file_json["prefix"] = Context::instance().target_prefix;
            // TODO: add other info here if necessary
            pid_file << file_json;
        }

        ~ScopedProcFile()
        {
            const auto lock = lock_proc_dir();
            std::error_code errcode;
            const bool is_removed = fs::remove(location, errcode);
            if (!is_removed)
            {
                LOG_WARNING << fmt::format(
                    "Failed to remove file '{}' : {}", location.string(), errcode.message());
            }
        }
    };

}

using namespace mamba;  // NOLINT(build/namespaces)

#ifndef _WIN32
void
daemonize()
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
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);

        if (fd > 2)
        {
            close(fd);
        }
    }
}
#endif

void
set_ps_command(CLI::App* subcom)
{
    auto list_subcom = subcom->add_subcommand("list");

    auto list_callback = []()
    {
        nlohmann::json info;
        {
            auto proc_dir_lock = lock_proc_dir();
            info = get_all_running_processes_info();
        }
        printers::Table table({ "PID", "Name", "Prefix", "Command" });
        table.set_padding({ 2, 4, 4, 4 });
        for (auto& el : info)
        {
            table.add_row({ el["pid"].get<std::string>(),
                            el["name"].get<std::string>(),
                            env_name(el["prefix"].get<std::string>()),
                            join(" ", el["command"].get<std::vector<std::string>>()) });
        }

        table.print(std::cout);
    };

    // ps is an alias for `ps list`
    list_subcom->callback(list_callback);
    subcom->callback(
        [subcom, list_subcom, list_callback]()
        {
            if (!subcom->got_subcommand(list_subcom))
                list_callback();
        });


    auto stop_subcom = subcom->add_subcommand("stop");
    static std::string pid_or_name;
    stop_subcom->add_option("pid_or_name", pid_or_name, "Process ID or process name (label)");
    stop_subcom->callback(
        []()
        {
            auto filter = [](const nlohmann::json& j) -> bool
            { return j["name"] == pid_or_name || j["pid"] == pid_or_name; };
            nlohmann::json procs;
            {
                auto proc_dir_lock = lock_proc_dir();
                procs = get_all_running_processes_info(filter);
            }

#ifndef _WIN32
            auto stop_process = [](const std::string& name, PID pid)
            {
                std::cout << fmt::format("Stopping {} [{}]", name, pid) << std::endl;
                kill(pid, SIGTERM);
            };
#else
            auto stop_process = [](const std::string& /*name*/, PID /*pid*/)
            { LOG_ERROR << "Process stopping not yet implemented on Windows."; };
#endif
            for (auto& p : procs)
            {
                PID pid = std::stoull(p["pid"].get<std::string>());
                stop_process(p["name"], pid);
            }
            if (procs.empty())
            {
                Console::instance().print("Did not find any matching process.");
                return -1;
            }
            return 0;
        });
}

void
set_run_command(CLI::App* subcom)
{
    init_prefix_options(subcom);

    static std::string streams;
    CLI::Option* stream_option
        = subcom
              ->add_option(
                  "-a,--attach",
                  streams,
                  "Attach to stdin, stdout and/or stderr. -a \"\" for disabling stream redirection")
              ->join(',');

    static std::string cwd;
    subcom->add_option(
        "--cwd", cwd, "Current working directory for command to run in. Defaults to cwd");

    static bool detach = false;
#ifndef _WIN32
    subcom->add_flag("-d,--detach", detach, "Detach process from terminal");
#endif

    static bool clean_env = false;
    subcom->add_flag("--clean-env", clean_env, "Start with a clean environment");

    static std::vector<std::string> env_vars;
    subcom->add_option("-e,--env", env_vars, "Add env vars with -e ENVVAR or -e ENVVAR=VALUE")
        ->allow_extra_args(false);

#ifndef _WIN32
    static std::string specific_process_name;
    subcom->add_option(
        "--label",
        specific_process_name,
        "Specifies the name of the process. If not set, a unique name will be generated derived from the executable name if possible.");
#endif

    subcom->prefix_command();

    static reproc::process proc;

    subcom->callback(
        [subcom, stream_option]()
        {
            auto& config = Configuration::instance();
            config.at("show_banner").set_value(false);
            config.load();
            bool const disable_lockfile = config.at("disable_lockfile").value<bool>();

            std::vector<std::string> command = subcom->remaining();
            if (command.empty())
            {
                LOG_ERROR << "Did not receive any command to run inside environment";
                exit(1);
            }

            // create a copy before inserting additional things
            std::vector<std::string> raw_command = command;

            // Make sure the proc directory is always existing and ready.
            fs::create_directories(proc_dir());

            LOG_DEBUG << "Currently running processes: " << get_all_running_processes_info();
            LOG_DEBUG << "Remaining args to run as command: " << join(" ", command);

        // replace the wrapping bash with new process entirely
#ifndef _WIN32
            if (command.front() != "exec")
                command.insert(command.begin(), "exec");
#endif

            auto [wrapped_command, script_file]
                = prepare_wrapped_call(Context::instance().target_prefix, command);

            LOG_DEBUG << "Running wrapped script: " << join(" ", command);

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
                            LOG_WARNING << "Requested env var " << e
                                        << " does not exist in environment";
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
                std::cout << fmt::format(fmt::fg(fmt::terminal_color::green),
                                         "Running wrapped script {} in the background",
                                         join(" ", command))
                          << std::endl;
                daemonize();
            }
#endif
            int status;
            {
#ifndef _WIN32
                // Lock the process directory to read and write in it until we are ready to launch
                // the child process.
                auto proc_dir_lock = disable_lockfile ? std::unique_ptr<LockFile>{} : lock_proc_dir();

                const std::string process_name = [&]
                {
                    // Insert a unique process name associated to the command, either specified by
                    // the user or generated.
                    command.reserve(4);  // We need at least 4 objects to not move around.

                    const auto exe_name_it = std::next(command.begin());
                    if (specific_process_name.empty())
                    {
                        const auto unique_name = generate_unique_process_name(*exe_name_it);
                        command.insert(exe_name_it, { { "-a" }, unique_name });
                        return unique_name;
                    }
                    else
                    {
                        if (is_process_name_running(specific_process_name))
                        {
                            throw std::runtime_error(
                                fmt::format("Another process with name '{}' is currently running.",
                                            specific_process_name));
                        }
                        command.insert(exe_name_it, { { "-a" }, specific_process_name });
                        return specific_process_name;
                    }
                }();

                // Writes the process file then unlock the directory. Deletes the process file once
                // exit is called (in the destructor).
                std::unique_ptr<ScopedProcFile> scoped_proc_file
                    = disable_lockfile ? std::unique_ptr<ScopedProcFile>{}
                              : std::make_unique<ScopedProcFile>(
                                  process_name, raw_command, std::move(proc_dir_lock));
#endif
                PID pid;
                std::error_code ec;

                ec = proc.start(wrapped_command, opt);

                std::tie(pid, ec) = proc.pid();

                if (ec)
                {
                    std::cerr << ec.message() << std::endl;
                    exit(1);
                }

#ifndef _WIN32
                MainExecutor::instance().schedule(
                    []()
                    {
                        signal(
                            SIGTERM,
                            [](int signum)
                            {
                                LOG_INFO
                                    << "Received SIGTERM on micromamba run - terminating process";
                                reproc::stop_actions sa;
                                sa.first = reproc::stop_action{ reproc::stop::terminate,
                                                                std::chrono::milliseconds(3000) };
                                sa.second = reproc::stop_action{ reproc::stop::kill,
                                                                 std::chrono::milliseconds(3000) };
                                proc.stop(sa);
                            });
                    });
#endif

                // check if we need this
                if (!opt.redirect.discard && opt.redirect.file == nullptr
                    && opt.redirect.path == nullptr)
                {
                    opt.redirect.parent = true;
                }

                ec = reproc::drain(proc, reproc::sink::null, reproc::sink::null);

                std::tie(status, ec) = proc.stop(opt.stop);

                if (ec)
                {
                    std::cerr << ec.message() << std::endl;
                }
            }
            // exit with status code from reproc
            exit(status);
        });
}
