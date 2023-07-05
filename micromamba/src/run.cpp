// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <csignal>
#include <exception>
#include <thread>

#include <fmt/color.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>

#include "mamba/api/configuration.hpp"
#include "mamba/api/install.hpp"
#include "mamba/core/error_handling.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/core/util_random.hpp"

#include "common_options.hpp"

#ifndef _WIN32
extern "C"
{
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
}
#else
#include <process.h>
#endif

#include "mamba/core/run.hpp"

using namespace mamba;  // NOLINT(build/namespaces)


void
set_ps_command(CLI::App* subcom)
{
    auto list_subcom = subcom->add_subcommand("list");

    auto list_callback = []()
    {
        nlohmann::json info;
        if (fs::is_directory(proc_dir()))
        {
            auto proc_dir_lock = lock_proc_dir();
            info = get_all_running_processes_info();
        }
        if (info.empty())
        {
            std::cout << "No running processes" << std::endl;
        }
        printers::Table table({ "PID", "Name", "Prefix", "Command" });
        table.set_padding({ 2, 4, 4, 4 });
        auto& context = Context::instance();  // REVIEW: this is temporary and should be deleted
                                              // before review.
        for (auto& el : info)
        {
            auto prefix = el["prefix"].get<std::string>();
            if (!prefix.empty())
            {
                prefix = env_name(context, prefix);
            }

            table.add_row({
                el["pid"].get<std::string>(),
                el["name"].get<std::string>(),
                prefix,
                fmt::format("{}", fmt::join(el["command"].get<std::vector<std::string>>(), " ")),
            });
        }

        table.print(std::cout);
    };

    // ps is an alias for `ps list`
    list_subcom->callback(list_callback);
    subcom->callback(
        [subcom, list_subcom, list_callback]()
        {
            if (!subcom->got_subcommand(list_subcom))
            {
                list_callback();
            }
        }
    );


    auto stop_subcom = subcom->add_subcommand("stop");
    static std::string pid_or_name;
    stop_subcom->add_option("pid_or_name", pid_or_name, "Process ID or process name (label)");
    stop_subcom->callback(
        []()
        {
            auto filter = [](const nlohmann::json& j) -> bool
            { return j["name"] == pid_or_name || j["pid"] == pid_or_name; };
            nlohmann::json procs;
            if (fs::is_directory(proc_dir()))
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
                PID pid = std::stoi(p["pid"].get<std::string>());
                stop_process(p["name"], pid);
            }
            if (procs.empty())
            {
                Console::instance().print("Did not find any matching process.");
                return -1;
            }
            return 0;
        }
    );
}

void
set_run_command(CLI::App* subcom, Configuration& config)
{
    init_prefix_options(subcom, config);

    static std::string streams;
    CLI::Option* stream_option = subcom
                                     ->add_option(
                                         "-a,--attach",
                                         streams,
                                         "Attach to stdin, stdout and/or stderr. -a \"\" for disabling stream redirection"
                                     )
                                     ->join(',');

    static std::string cwd;
    subcom->add_option("--cwd", cwd, "Current working directory for command to run in. Defaults to cwd");

    static bool detach = false;
#ifndef _WIN32
    subcom->add_flag("-d,--detach", detach, "Detach process from terminal");
#endif

    static bool clean_env = false;
    subcom->add_flag("--clean-env", clean_env, "Start with a clean environment");

    static std::vector<std::string> env_vars;
    subcom->add_option("-e,--env", env_vars, "Add env vars with -e ENVVAR or -e ENVVAR=VALUE")
        ->allow_extra_args(false);

    static std::string specific_process_name;
#ifndef _WIN32
    subcom->add_option(
        "--label",
        specific_process_name,
        "Specifies the name of the process. If not set, a unique name will be generated derived from the executable name if possible."
    );
#endif

    subcom->prefix_command();

    static reproc::process proc;

    subcom->callback(
        [&config, subcom, stream_option]()
        {
            config.load();

            std::vector<std::string> command = subcom->remaining();
            if (command.empty())
            {
                LOG_ERROR << "Did not receive any command to run inside environment";
                exit(1);
            }

            // create a copy before inserting additional things
            std::vector<std::string> raw_command = command;

        // replace the wrapping bash with new process entirely
#ifndef _WIN32
            if (command.front() != "exec")
                command.insert(command.begin(), "exec");
#endif

            bool all_streams = stream_option->count() == 0u;
            bool sinkout = !all_streams && streams.find("stdout") == std::string::npos;
            bool sinkerr = !all_streams && streams.find("stderr") == std::string::npos;
            bool sinkin = !all_streams && streams.find("stdin") == std::string::npos;

            int stream_options = 0;
            if (!all_streams)
            {
                stream_options = (sinkout ? 0 : static_cast<int>(STREAM_OPTIONS::SINKOUT));
                stream_options |= (sinkerr ? 0 : static_cast<int>(STREAM_OPTIONS::SINKERR));
                stream_options |= (sinkin ? 0 : static_cast<int>(STREAM_OPTIONS::SINKIN));
            }

            auto const get_prefix = [&]()
            {
                auto& ctx = Context::instance();
                if (auto prefix = ctx.prefix_params.target_prefix; !prefix.empty())
                {
                    return prefix;
                }
                return ctx.prefix_params.root_prefix;
            };

            int exit_code = mamba::run_in_environment(
                get_prefix(),
                command,
                cwd,
                stream_options,
                clean_env,
                detach,
                env_vars,
                specific_process_name
            );

            exit(exit_code);
        }
    );
}
