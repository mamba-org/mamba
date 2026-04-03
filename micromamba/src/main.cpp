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

#include <algorithm>

#include <CLI/CLI.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/spdlog/logging_spdlog.hpp"
#include "mamba/version.hpp"

#include "umamba.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

auto
decide_preconfig_context_options(int argc, char** argv) -> mamba::ContextOptions
{
    using namespace std::literals;

    mamba::ContextOptions options{
        .enable_logging = true,
        .enable_signal_handling = true,
    };

    mamba::OutputParams output_params;
    for (const char* arg : std::ranges::subrange(argv, argv + argc))
    {
        if (arg == "--json"sv)
        {
            output_params.json = true;
        }
        else if (arg == "--quiet"sv)
        {
            output_params.quiet = true;
        }
    }

    if (output_params.json or output_params.quiet)
    {
        options.output_params = output_params;
    }

    return options;
}

auto
decide_log_handler(const ContextOptions& options) -> mamba::logging::AnyLogHandler
{
    if (options.output_params and (options.output_params->json or options.output_params->quiet))
    {
        return {};
    }

    return mamba::logging::spdlogimpl::LogHandler_spdlog{};
}

int
main(int argc, char** argv)
{
    mamba::MainExecutor scoped_threads;
    const auto pre_config_options = decide_preconfig_context_options(argc, argv);
    mamba::Context ctx{ pre_config_options, decide_log_handler(pre_config_options) };
    mamba::Console console{ ctx };
    mamba::Configuration config{ ctx };

    init_console();
    mamba::on_scope_exit _console_reset{ [] { reset_console(); } };

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
    auto handle_exception = [&](auto& e, const auto&... additional_messages)
    {
        using namespace std::literals;
        error_to_report.emplace(e.what());
        (error_to_report->append(additional_messages), ...);
        set_sig_interrupted();
    };

    int return_value = EXIT_SUCCESS;

    try
    {
        // Note: do not use CLI11_PARSE macro as it's error handling
        // would bypass ours.
        app.parse(argc, utf8argv);
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
    catch (const mamba::mamba_error& e)
    {
        // We treat interruptions (ctrl-c) specially by not logging a critical error.s
        const bool is_interruption = [&]
        {
            if (e.error_code() == mamba::mamba_error_code::aggregated)
            {
                const auto& aggregated_error = static_cast<const mamba::mamba_aggregated_error&>(e);
                return aggregated_error.has_only_error(mamba::mamba_error_code::user_interrupted);
            }
            else
            {
                return e.error_code() == mamba::mamba_error_code::user_interrupted;
            }
        }();

        if (is_interruption)
        {
            LOG_WARNING << e.what();
            return 0;
        }
        else
        {
            handle_exception(e);
        }
    }
    catch (const CLI::Error& e)
    {
        using namespace std::literals;
        // We only preserve CLI11 output behavior when errors from CLI11
        // occurs because of `--help` or `--version` is used. Otherwise we follow the
        // logic that `--json` outpus everything as JSON.
        static constexpr std::array non_error_request_names = { "CallForHelp"sv,
                                                                "CallForAllHelp"sv,
                                                                "CallForVersion"sv };
        const bool is_non_error_request = std::ranges::find(non_error_request_names, e.get_name())
                                          != non_error_request_names.end();

        if (ctx.output_params.json and not is_non_error_request)
        {
            // we want the output to end up in the json log history
            std::stringstream output;
            return_value = app.exit(e, output, output);
            LOG_WARNING << output.str();
        }
        else
        {
            // we don't want any json output even if requested, CLI11 will handle this
            console.cancel_json_print();
            return_value = app.exit(e);
        }
    }
    catch (const std::exception& e)
    {
        handle_exception(e);
    }

    if (error_to_report)
    {
        LOG_CRITICAL << error_to_report.value();
        return_value = EXIT_FAILURE;
    }

    return return_value;
}
