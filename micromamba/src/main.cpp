// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "umamba.hpp"

#include "mamba/version.hpp"

#include "mamba/api/configuration.hpp"

#include "mamba/core/context.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util_os.hpp"

#include <CLI/CLI.hpp>


using namespace mamba;  // NOLINT(build/namespaces)


int
main(int argc, char** argv)
{
    init_console();
    auto& ctx = Context::instance();

    ctx.is_micromamba = true;
    ctx.custom_banner = banner;

    CLI::App app{ "Version: " + version() + "\n" };
    set_umamba_command(&app);

    char** utf8argv;

#ifdef _WIN32
    wchar_t** wargv;
    wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::vector<std::string> utf8Args;
    std::vector<char*> utf8CharArgs;
    for (int i = 0; i < argc; i++)
    {
        utf8Args.push_back(to_utf8(wargv[i]));
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
        get_completions(&app, argc, utf8argv);
        reset_console();
        return 0;
    }

    std::stringstream full_command;
    for (int i = 0; i < argc; ++i)
    {
        full_command << utf8argv[i];
        if (i < argc - 1)
            full_command << " ";
    }
    ctx.current_command = full_command.str();

    bool err = false;
    try
    {
        CLI11_PARSE(app, argc, utf8argv);
        if (app.get_subcommands().size() == 0)
        {
            Configuration::instance().load();
            Console::print(app.help());
        }
        if (app.got_subcommand("config")
            && app.get_subcommand("config")->get_subcommands().size() == 0)
        {
            Configuration::instance().load();
            Console::print(app.get_subcommand("config")->help());
        }
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL << e.what();
        set_sig_interrupted();
        err = false;
    }
    reset_console();
    return err ? 1 : 0;
}
