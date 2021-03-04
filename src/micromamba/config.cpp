// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "config.hpp"
#include "parsers.hpp"

#include "mamba/config.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_config_parser(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);
}

void
init_config_list_parser(CLI::App* subcom)
{
    init_config_parser(subcom);

    subcom->add_flag("--show-source",
                     config_options.show_sources,
                     "Display all identified configuration sources.");
}

void
load_config_options(Context& ctx)
{
    load_general_options(ctx);
    load_prefix_options(ctx);
    load_rc_options(ctx);
}

void
set_config_list_command(CLI::App* subcom)
{
    init_config_list_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        load_config_options(ctx);

        if (general_options.no_rc)
        {
            std::cout << "Configuration files disabled by --no-rc flag" << std::endl;
        }
        else
        {
            Configurable& config = Configurable::instance();
            std::cout << config.dump(config_options.show_sources) << std::endl;
        }
        return 0;
    });
}

void
set_config_sources_command(CLI::App* subcom)
{
    init_config_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        load_config_options(ctx);

        if (general_options.no_rc)
        {
            std::cout << "Configuration files disabled by --no-rc flag" << std::endl;
        }
        else
        {
            std::cout << "Configuration files (by precedence order):" << std::endl;

            Configurable& config = Configurable::instance();
            auto srcs = config.get_sources();
            auto valid_srcs = config.get_valid_sources();

            for (auto s : srcs)
            {
                auto found_s = std::find(valid_srcs.begin(), valid_srcs.end(), s);
                if (found_s != valid_srcs.end())
                {
                    std::cout << env::shrink_user(s).string() << std::endl;
                }
                else
                {
                    std::cout << env::shrink_user(s).string() + " (invalid)" << std::endl;
                }
            }
        }
        return 0;
    });
}

void
set_config_command(CLI::App* subcom)
{
    init_config_parser(subcom);

    auto list_subcom = subcom->add_subcommand("list", "List configuration values");
    set_config_list_command(list_subcom);

    auto sources_subcom = subcom->add_subcommand("sources", "Show configuration sources");
    set_config_sources_command(sources_subcom);
}
