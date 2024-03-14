// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifdef _WIN32  // This set of includes is requires for CommandLineToArgvW() to be available.
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <windows.h>
// Incomplete header included last
#include <shellapi.h>

#include "mamba/util/os_win.hpp"
#endif

#include <CLI/CLI.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/version.hpp"

#include "umamba.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

int
main(int argc, char** argv)
{
    mamba::MainExecutor scoped_threads;
    mamba::Context ctx{ {
        /* .enable_logging_and_signal_handling = */ true,
    } };
    mamba::Console console{ ctx };
    mamba::Configuration config{ ctx };

    init_console();

    ctx.command_params.is_mamba_exe = true;

    CLI::App app{ "Version: " + version() + "\n" };
    set_umamba_command(&app, config);

    char** utf8argv;

#ifdef _WIN32
    wchar_t** wargv;
    wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::vector<std::string> utf8Args;
    std::vector<char*> utf8CharArgs;
    for (int i = 0; i < argc; i++)
    {
        utf8Args.push_back(util::windows_encoding_to_utf8(wargv[i]));
    }
    for (int i = 0; i < argc; ++i)
    {
        utf8CharArgs.push_back(utf8Args[i].data());
    }
    utf8argv = utf8CharArgs.data();
#else
    utf8argv = argv;
#endif

    if (argc >= 2 && strcmp(argv[1], "completer") == 0)
    {
        get_completions(&app, config, argc, utf8argv);
        reset_console();
        return 0;
    }

    std::stringstream full_command;
    for (int i = 0; i < argc; ++i)
    {
        full_command << utf8argv[i];
        if (i < argc - 1)
        {
            full_command << " ";
        }
    }
    ctx.command_params.current_command = full_command.str();

    std::optional<std::string> error_to_report;
    try
    {
        CLI11_PARSE(app, argc, utf8argv);
        if (app.get_subcommands().size() == 0)
        {
            config.load();
            Console::instance().print(app.help());
        }
        if (app.got_subcommand("config")
            && app.get_subcommand("config")->get_subcommands().size() == 0)
        {
            config.load();
            Console::instance().print(app.get_subcommand("config")->help());
        }
    }
    catch (const std::exception& e)
    {
        error_to_report = e.what();
        set_sig_interrupted();
    }

    reset_console();

    if (error_to_report)
    {
        LOG_CRITICAL << error_to_report.value();
        return 1;  // TODO: consider returning EXIT_FAILURE
    }

    return 0;
}
