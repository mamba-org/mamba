#include "mamba/api/configuration.hpp"

#include "mamba/core/environments_manager.hpp"
#include "mamba/core/output.hpp"


std::vector<std::string> first_level_completions = { "activate", "deactivate", "install",
                                                     "remove",   "update",     "list",
                                                     "clean",    "config",     "info" };

bool
string_comparison(const std::string& a, const std::string& b)
{
    return a < b;
}

void
complete_options(CLI::App* app, const std::vector<std::string>& last_args, bool& completed)
{
    if (completed || last_args.empty())
        return;
    completed = true;

    if (last_args[0] == "-n" && last_args.size() == 2)
    {
        auto& config = mamba::Configuration::instance();
        config.at("show_banner").set_value(false);
        config.load();

        auto root_prefix = config.at("root_prefix").value<fs::path>();
        auto& name_start = last_args.back();

        std::vector<std::string> prefixes;
        if (fs::exists(root_prefix / "envs"))
            for (const auto& p : fs::directory_iterator(root_prefix / "envs"))
                if (p.is_directory() && fs::exists(p / "conda-meta"))
                {
                    auto name = p.path().filename().string();
                    if (mamba::starts_with(name, name_start))
                        prefixes.push_back(name);
                }

        std::sort(prefixes.begin(), prefixes.end(), string_comparison);
        std::cout << mamba::printers::table_like(prefixes, 90).str() << std::endl;
    }
    else if (mamba::starts_with(last_args.back(), "-"))
    {
        auto opt_start = mamba::lstrip(last_args.back(), "-");

        if (opt_start.empty())
            return;

        std::vector<std::string> options;
        if (mamba::starts_with(last_args.back(), "--"))
            for (const auto* opt : app->get_options())
            {
                for (const auto& n : opt->get_lnames())
                    if (mamba::starts_with(n, opt_start))
                        options.push_back("--" + n);
            }
        else
            for (const auto* opt : app->get_options())
            {
                for (const auto& n : opt->get_snames())
                    if (mamba::starts_with(n, opt_start))
                        options.push_back("-" + n);
            }

        // std::cout << mamba::join(" ", options) << std::endl;
        std::cout << mamba::printers::table_like(options, 90).str() << std::endl;
    }
}

int
get_completions(CLI::App* app, int argc, char** argv)
{
    std::vector<std::string> completer_args;
    bool completed = false;

    CLI::App* install_subcom = app->get_subcommand("install");

    if (argc > 2 && std::string(argv[argc - 2]) == "-n")
    {
        completer_args.push_back(argv[argc - 2]);
        completer_args.push_back(argv[argc - 1]);
        argc -= 1;
    }
    else
        completer_args.push_back(argv[argc - 1]);

    app->callback([&]() { complete_options(app, completer_args, completed); });
    install_subcom->callback(
        [&]() { complete_options(install_subcom, completer_args, completed); });

    argv[1] = argv[0];
    CLI11_PARSE(*app, argc - 2, argv + 1);

    exit(0);
}
