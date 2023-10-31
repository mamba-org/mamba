// Copyright (c) 2020, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <array>
#include <csignal>
#include <iostream>
#include <string>
#include <vector>

#ifndef _WIN32
extern "C"
{
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}
#else
#include <process.h>
#endif

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <nlohmann/json.hpp>
#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>

#include "mamba/core/context.hpp"
#include "mamba/core/environment.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/run.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/util_random.hpp"
#include "mamba/util/environment.hpp"
#include "mamba/util/string.hpp"


namespace mamba
{

    std::string generate_unique_process_name(std::string_view program_name)
    {
        assert(!program_name.empty());

        static constexpr std::array prefixes = {
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

        static std::vector prefixes_bag(prefixes.cbegin(), prefixes.cend());
        std::string selected_name{ program_name };
        while (true)
        {
            std::string selected_prefix;
            if (!prefixes_bag.empty())
            {
                // Pick a random prefix from our bag of prefixes.
                const auto selected_prefix_idx = random_int<std::size_t>(0, prefixes_bag.size() - 1);
                const auto selected_prefix_it = std::next(
                    prefixes_bag.begin(),
                    static_cast<std::ptrdiff_t>(selected_prefix_idx)
                );
                selected_prefix = *selected_prefix_it;
                prefixes_bag.erase(selected_prefix_it);
            }
            else if (!alt_names.empty())
            {
                // No more prefixes: we retry the same prefixes but with a different program name.
                const auto selected_name_idx = random_int<std::size_t>(0, alt_names.size() - 1);
                const auto selected_name_it = std::next(
                    alt_names.begin(),
                    static_cast<std::ptrdiff_t>(selected_name_idx)
                );
                selected_name = *selected_name_it;
                alt_names.erase(selected_name_it);
                // Re-fill the prefix bag.
                prefixes_bag = decltype(prefixes_bag)(prefixes.cbegin(), prefixes.cend());
                continue;  // Re-try with new prefix + new name.
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
            {
                return new_process_name;
            }
        }
    }

    const fs::u8path& proc_dir()
    {
        static auto path = env::user_cache_dir() / "proc";
        return path;
    }

    LockFile lock_proc_dir()
    {
        const auto proc_dir_path = proc_dir();
        auto lockfile = LockFile(proc_dir_path);
        if (!lockfile)
        {
            if (auto error = lockfile.error())
            {
                throw mamba_error{
                    fmt::format(
                        "'mamba run' failed to lock ({}) or lockfile was not properly deleted - error: {}",
                        proc_dir_path.string(),
                        error->what()
                    ),
                    mamba_error_code::lockfile_failure
                };
            }
            else
            {
                LOG_DEBUG << "`mamba run` file locking attempt ignored because locking is disabled - path: "
                          << proc_dir_path.string();
            }
        }

        return lockfile;
    }

    nlohmann::json
    get_all_running_processes_info(const std::function<bool(const nlohmann::json&)>& filter)
    {
        nlohmann::json all_processes_info;

        const auto open_mode = std::ios::binary | std::ios::in;

        for (auto&& entry : fs::directory_iterator{ proc_dir() })
        {
            const auto file_location = entry.path();
            if (file_location.extension() != ".json")
            {
                continue;
            }

            std::ifstream pid_file{ file_location.std_path(), open_mode };
            if (!pid_file.is_open())
            {
                LOG_WARNING << fmt::format("failed to open {}", file_location.string());
                continue;
            }

            auto running_processes_info = nlohmann::json::parse(pid_file);
            running_processes_info["pid"] = file_location.filename().replace_extension().string();
            if (!filter || filter(running_processes_info))
            {
                all_processes_info.push_back(running_processes_info);
            }
        }

        return all_processes_info;
    }

    bool is_process_name_running(const std::string& name)
    {
        const auto other_processes_with_same_name = get_all_running_processes_info(
            [&](const nlohmann::json& process_info) { return process_info["name"] == name; }
        );
        return !other_processes_with_same_name.empty();
    }

    ScopedProcFile::ScopedProcFile(
        const Context& context,
        const std::string& name,
        const std::vector<std::string>& command,
        LockFile proc_dir_lock
    )
        : location{ proc_dir() / fmt::format("{}.json", getpid()) }
    {
        // Lock must be hold for the duraction of this constructor.
        if (is_file_locking_allowed())
        {
            assert(proc_dir_lock);
        }

        const auto open_mode = std::ios::binary | std::ios::trunc | std::ios::out;
        std::ofstream pid_file(location.std_path(), open_mode);
        if (!pid_file.is_open())
        {
            throw std::runtime_error(
                fmt::format("'mamba run' failed to open/create file: {}", location.string())
            );
        }

        nlohmann::json file_json;
        file_json["name"] = name;
        file_json["command"] = command;
        file_json["prefix"] = context.prefix_params.target_prefix.string();
        // TODO: add other info here if necessary
        pid_file << file_json;
    }

    ScopedProcFile::~ScopedProcFile()
    {
        const auto lock = lock_proc_dir();
        std::error_code errcode;
        const bool is_removed = fs::remove(location, errcode);
        if (!is_removed)
        {
            LOG_WARNING << fmt::format(
                "Failed to remove file '{}' : {}",
                location.string(),
                errcode.message()
            );
        }
    }

#ifndef _WIN32
    void daemonize()
    {
        pid_t pid, sid;
        int fd;

        // already a daemon
        if (getppid() == 1)
        {
            return;
        }

        // fork parent process
        pid = fork();
        if (pid < 0)
        {
            exit(1);
        }

        // exit parent process
        if (pid > 0)
        {
            exit(0);
        }

        // at this point we are executing as the child process
        // create a new SID for the child process
        sid = setsid();
        if (sid < 0)
        {
            exit(1);
        }

        fd = open("/dev/null", O_RDWR, 0);

        auto out = Console::stream();
        fmt::print(out, "Kill process with: kill {}\n", getpid());

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

    int run_in_environment(
        const Context& context,
        const fs::u8path& prefix,
        std::vector<std::string> command,
        const std::string& cwd,
        int stream_options,
        bool clean_env,
        bool detach,
        const std::vector<std::string>& env_vars,
        const std::string& specific_process_name
    )
    {
        if (!fs::exists(prefix))
        {
            LOG_CRITICAL << "The given prefix does not exist: " << prefix;
            return 1;
        }
        std::vector<std::string> raw_command = command;
        // Make sure the proc directory is always existing and ready.
        std::error_code ec;
        bool is_created = fs::create_directories(proc_dir(), ec);
        if (!is_created && ec)
        {
            LOG_WARNING << "Could not create proc dir: " << proc_dir() << " (" << ec.message() << ")";
        }

        LOG_DEBUG << "Currently running processes: " << get_all_running_processes_info();
        fmt::print(LOG_DEBUG, "Remaining args to run as command: {}", fmt::join(command, " "));

        // replace the wrapping bash with new process entirely
#ifndef _WIN32
        if (command.front() != "exec")
        {
            command.insert(command.begin(), "exec");
        }
#endif

        auto [wrapped_command, script_file] = prepare_wrapped_call(context, prefix, command);

        fmt::print(LOG_DEBUG, "Running wrapped script: {}", fmt::join(command, " "));

        bool sinkout = stream_options & static_cast<int>(STREAM_OPTIONS::SINKOUT);
        bool sinkerr = stream_options & static_cast<int>(STREAM_OPTIONS::SINKERR);
        bool sinkin = stream_options & static_cast<int>(STREAM_OPTIONS::SINKIN);

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
                    auto split_e = util::split(e, "=", 1);
                    env_map[split_e[0]] = split_e[1];
                }
                else
                {
                    auto val = util::getenv(e);
                    if (val)
                    {
                        env_map[e] = val.value();
                    }
                    else
                    {
                        LOG_WARNING << "Requested env var " << e << " does not exist in environment";
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
            Console::stream() << fmt::format(
                context.graphics_params.palette.success,
                "Running wrapped script {} in the background\n",
                fmt::join(command, " ")
            );
            daemonize();
        }
#endif
        int status;
        {
#ifndef _WIN32
            // Lock the process directory to read and write in it until we are ready to launch
            // the child process.
            auto proc_dir_lock = lock_proc_dir();

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
                        throw std::runtime_error(fmt::format(
                            "Another process with name '{}' is currently running.",
                            specific_process_name
                        ));
                    }
                    command.insert(exe_name_it, { { "-a" }, specific_process_name });
                    return specific_process_name;
                }
            }();

            // Writes the process file then unlock the directory. Deletes the process file once
            // exit is called (in the destructor).
            std::unique_ptr<ScopedProcFile> scoped_proc_file = nullptr;
            if (fs::is_directory(proc_dir()) && mamba::path::is_writable(proc_dir()))
            {
                scoped_proc_file = std::make_unique<ScopedProcFile>(
                    context,
                    process_name,
                    raw_command,
                    std::move(proc_dir_lock)
                );
            }
#endif
            PID pid;
            std::error_code lec;
            static reproc::process proc;

            lec = proc.start(wrapped_command, opt);

            std::tie(pid, lec) = proc.pid();

            if (lec)
            {
                std::cerr << ec.message() << '\n';
                return 1;
            }

#ifndef _WIN32
            MainExecutor::instance().schedule(
                []()
                {
                    signal(
                        SIGTERM,
                        [](int /*signum*/)
                        {
                            LOG_INFO << "Received SIGTERM on micromamba run - terminating process";
                            reproc::stop_actions sa;
                            sa.first = reproc::stop_action{ reproc::stop::terminate,
                                                            std::chrono::milliseconds(3000) };
                            sa.second = reproc::stop_action{ reproc::stop::kill,
                                                             std::chrono::milliseconds(3000) };
                            proc.stop(sa);
                        }
                    );
                }
            );
#endif

            // check if we need this
            if (!opt.redirect.discard && opt.redirect.file == nullptr && opt.redirect.path == nullptr)
            {
                opt.redirect.parent = true;
            }

            ec = reproc::drain(proc, reproc::sink::null, reproc::sink::null);

            std::tie(status, ec) = proc.stop(opt.stop);

            if (ec)
            {
                std::cerr << ec.message() << " ; error code " << ec.value() << '\n';
            }
        }
        // exit with status code from reproc
        return status;
    }
}
