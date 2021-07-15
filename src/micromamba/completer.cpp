#include "mamba/api/configuration.hpp"

#include "mamba/core/output.hpp"

#include <CLI/CLI.hpp>


void
complete_options(CLI::App* app, const std::vector<std::string>& last_args, bool& completed)
{
    if (completed || last_args.empty())
        return;

    completed = true;
    std::vector<std::string> options;

    if (last_args[0] == "-n" && last_args.size() == 2)
    {
        auto& config = mamba::Configuration::instance();
        config.at("show_banner").set_value(false);
        config.load();

        auto root_prefix = config.at("root_prefix").value<fs::path>();
        auto& name_start = last_args.back();

        if (fs::exists(root_prefix / "envs"))
            for (const auto& p : fs::directory_iterator(root_prefix / "envs"))
                if (p.is_directory() && fs::exists(p / "conda-meta"))
                {
                    auto name = p.path().filename().string();
                    if (mamba::starts_with(name, name_start))
                        options.push_back(name);
                }
    }
    else if (mamba::starts_with(last_args.back(), "-"))
    {
        auto opt_start = mamba::lstrip(last_args.back(), "-");

        if (mamba::starts_with(last_args.back(), "--"))
            for (const auto* opt : app->get_options())
            {
                for (const auto& n : opt->get_lnames())
                    if (mamba::starts_with(n, opt_start))
                        options.push_back("--" + n);
            }
        else
        {
            if (opt_start.empty())
                return;
            for (const auto* opt : app->get_options())
            {
                for (const auto& n : opt->get_snames())
                    if (mamba::starts_with(n, opt_start))
                        options.push_back("-" + n);
            }
        }
    }
    else
    {
        for (const auto* subc : app->get_subcommands(nullptr))
        {
            auto& n = subc->get_name();
            if (mamba::starts_with(n, last_args.back()))
                options.push_back(n);
        }
    }

    std::cout << mamba::printers::table_like(options, 90).str() << std::endl;
}

void
overwrite_callbacks(std::vector<CLI::App*>& apps,
                    const std::vector<std::string>& completer_args,
                    bool& completed)
{
    auto* app = apps.back();
    app->callback(
        [app, &completer_args, &completed]() { complete_options(app, completer_args, completed); });
    for (auto* subc : app->get_subcommands(nullptr))
    {
        apps.push_back(subc);
        overwrite_callbacks(apps, completer_args, completed);
    }
}

void
get_completions(CLI::App* app, int argc, char** argv)
{
    std::vector<std::string> completer_args;
    bool completed = false;

    if (argc > 2 && std::string(argv[argc - 2]) == "-n")
    {
        completer_args.push_back(argv[argc - 2]);
        completer_args.push_back(std::string(mamba::strip(argv[argc - 1])));
        argc -= 1;  // don't parse the -n
    }
    else
        completer_args.push_back(std::string(mamba::strip(argv[argc - 1])));

    std::vector<CLI::App*> apps = { app };
    overwrite_callbacks(apps, completer_args, completed);
    argv[1] = argv[0];

    try
    {
        app->parse(argc - 2, argv + 1);
    }
    catch (...)
    {
    }
}
