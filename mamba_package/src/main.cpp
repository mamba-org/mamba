// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <CLI/CLI.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/core/context.hpp"
#include "mamba/core/execution.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/thread_utils.hpp"
#include "mamba/core/util_os.hpp"
#include "mamba/version.hpp"

#include "package.hpp"

int
main(int argc, char** argv)
{
    using namespace mamba;  // NOLINT(build/namespaces)

    MainExecutor main_executor;
    Context context{ { /* .enable_blah_blah = */ true } };
    Console console{ context };
    Configuration config{ context };

    // call init console to setup utf8 extraction
    init_console();

    CLI::App app{ "Version: " + version() + "\n" };
    set_package_command(&app, context);

    try
    {
        CLI11_PARSE(app, argc, argv);
    }
    catch (const std::exception& e)
    {
        LOG_CRITICAL << e.what();
        set_sig_interrupted();
        return 1;
    }


    if (app.get_subcommands().size() == 0)
    {
        config.load();
        console.print(app.help());
    }

    return 0;
}
