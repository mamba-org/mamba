// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "mamba/api/configuration.hpp"
#include "mamba/core/output.hpp"
#include "mamba/core/run.hpp"
#include "mamba/fs/filesystem.hpp"
#include "mamba/util/string.hpp"

void
complete_options(
    CLI::App* app,
    mamba::Configuration& config,
    const std::vector<std::string>& last_args,
    bool& completed
)
{
    if (completed || last_args.empty())
    {
        return;
    }

    completed = true;
    std::vector<std::string> options;

    if (last_args[0] == "-n" && last_args.size() == 2)
    {
        config.load();

        auto root_prefix = config.at("root_prefix").value<mamba::fs::u8path>();
        auto& name_start = last_args.back();

        if (mamba::fs::exists(root_prefix / "envs"))
        {
            for (const auto& p : mamba::fs::directory_iterator(root_prefix / "envs"))
            {
                if (p.is_directory() && mamba::fs::exists(p.path() / "conda-meta"))
                {
                    auto name = p.path().filename().string();
                    if (mamba::util::starts_with(name, name_start))
                    {
                        options.push_back(name);
                    }
                }
            }
        }
    }
    else if (mamba::util::starts_with(last_args.back(), "-"))
    {
        auto opt_start = mamba::util::lstrip(last_args.back(), "-");

        if (mamba::util::starts_with(last_args.back(), "--"))
        {
            for (const auto* opt : app->get_options())
            {
                for (const auto& n : opt->get_lnames())
                {
                    if (mamba::util::starts_with(n, opt_start))
                    {
                        options.push_back("--" + n);
                    }
                }
            }
        }
        else
        {
            if (opt_start.empty())
            {
                return;
            }
            for (const auto* opt : app->get_options())
            {
                for (const auto& n : opt->get_snames())
                {
                    if (mamba::util::starts_with(n, opt_start))
                    {
                        options.push_back("-" + n);
                    }
                }
            }
        }
    }
    else
    {
        for (const auto* subc : app->get_subcommands(nullptr))
        {
            auto& n = subc->get_name();
            if (mamba::util::starts_with(n, last_args.back()))
            {
                options.push_back(n);
            }
        }
    }

    std::cout << mamba::printers::table_like(options, 90).str() << std::endl;
}

void
overwrite_callbacks(
    std::vector<CLI::App*>& apps,
    mamba::Configuration& config,
    const std::vector<std::string>& completer_args,
    bool& completed
)
{
    auto* app = apps.back();
    app->callback([app, &completer_args, &completed, &config]()
                  { complete_options(app, config, completer_args, completed); });
    for (auto* subc : app->get_subcommands(nullptr))
    {
        apps.push_back(subc);
        overwrite_callbacks(apps, config, completer_args, completed);
    }
}

void
add_activate_completion(
    CLI::App* app,
    mamba::Configuration& config,
    std::vector<std::string>& completer_args,
    bool& completed
)
{
    auto* current_subcom = app->get_subcommand("activate");
    app->remove_subcommand(current_subcom);

    // Mock functions just for completion
    CLI::App* activate_subcom = app->add_subcommand("activate");
    app->add_subcommand("deactivate");
    activate_subcom->callback(
        [app, &completer_args, &completed, &config]()
        {
            if (completer_args.size() == 1)
            {
                completer_args = { "-n", completer_args.back() };
                complete_options(app, config, completer_args, completed);
            }
        }
    );
}

void
add_ps_completion(
    CLI::App* app,
    mamba::Configuration& config,
    std::vector<std::string>& completer_args,
    bool& completed
)
{
    auto* current_subcom = app->get_subcommand("ps");
    app->remove_subcommand(current_subcom);

    // Mock functions just for completion
    CLI::App* ps_subcom = app->add_subcommand("ps");

    CLI::App* stop_subcom = ps_subcom->add_subcommand("stop");

    CLI::App* list_subcom = ps_subcom->add_subcommand("list");

    ps_subcom->callback([ps_subcom, &completer_args, &completed, &config]()
                        { complete_options(ps_subcom, config, completer_args, completed); });

    list_subcom->callback([list_subcom, &completer_args, &completed, &config]()
                          { complete_options(list_subcom, config, completer_args, completed); });

    stop_subcom->callback(
        [&completer_args, &completed]()
        {
            if (completer_args.size() == 1)
            {
                nlohmann::json info;
                {
                    auto proc_lock = mamba::lock_proc_dir();
                    info = mamba::get_all_running_processes_info();
                }
                std::vector<std::string> procs;
                for (auto& i : info)
                {
                    procs.push_back(i["name"].get<std::string>());
                }

                completed = true;
                std::cout << mamba::printers::table_like(procs, 90).str() << std::endl;
            }
        }
    );
}

void
get_completions(CLI::App* app, mamba::Configuration& config, int argc, char** argv)
{
    std::vector<std::string> completer_args;
    bool completed = false;

    if (argc > 2 && std::string(argv[argc - 2]) == "-n")
    {
        completer_args.push_back(argv[argc - 2]);
        completer_args.push_back(std::string(mamba::util::strip(argv[argc - 1])));
        argc -= 1;  // don't parse the -n
    }
    else
    {
        completer_args.push_back(std::string(mamba::util::strip(argv[argc - 1])));
    }

    std::vector<CLI::App*> apps = { app };
    overwrite_callbacks(apps, config, completer_args, completed);
    add_activate_completion(app, config, completer_args, completed);
    add_ps_completion(app, config, completer_args, completed);
    argv[1] = argv[0];

    try
    {
        app->parse(argc - 2, argv + 1);
    }
    catch (...)
    {
    }
}
