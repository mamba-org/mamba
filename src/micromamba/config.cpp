// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_config_options(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);

    auto& config = Configuration::instance();

    config.insert(Configurable("config_specs", std::vector<std::string>({}))
                      .group("cli")
                      .rc_configurable(false)
                      .description("Configurables show"));

    config.insert(Configurable("config_show_long_descriptions", false)
                      .group("cli")
                      .rc_configurable(false)
                      .description("Display configurables long descriptions"));

    config.insert(Configurable("config_show_groups", false)
                      .group("cli")
                      .rc_configurable(false)
                      .description("Display configurables groups"));
}

void
init_config_list_options(CLI::App* subcom)
{
    init_config_options(subcom);

    auto& config = Configuration::instance();

    auto& specs = config.at("config_specs").get_wrapped<std::vector<std::string>>();
    subcom->add_option("specs", specs.set_cli_config({}), specs.description());

    auto& show_sources
        = config.insert(Configurable("config_show_sources", false)
                            .group("cli")
                            .rc_configurable(false)
                            .description("Display all identified configuration sources"));
    subcom->add_flag("-s,--sources", show_sources.set_cli_config(0), show_sources.description());

    auto& show_all
        = config.insert(Configurable("config_show_all", false)
                            .group("cli")
                            .rc_configurable(false)
                            .description("Display all configuration values, including defaults"));
    subcom->add_flag("-a,--all", show_all.set_cli_config(0), show_all.description());

    auto& show_description = config.insert(Configurable("config_show_descriptions", false)
                                               .group("cli")
                                               .rc_configurable(false)
                                               .description("Display configurables descriptions"));
    subcom->add_flag(
        "-d,--descriptions", show_description.set_cli_config(0), show_description.description());

    auto& show_long_description = config.at("config_show_long_descriptions").get_wrapped<bool>();
    subcom->add_flag("-l,--long-descriptions",
                     show_long_description.set_cli_config(0),
                     show_long_description.description());

    auto& show_groups = config.at("config_show_groups").get_wrapped<bool>();
    subcom->add_flag("-g,--groups", show_groups.set_cli_config(0), show_groups.description());
}

void
init_config_describe_options(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& specs = config.at("config_specs").get_wrapped<std::vector<std::string>>();
    subcom->add_option("specs", specs.set_cli_config({}), specs.description());

    auto& show_long_description = config.at("config_show_long_descriptions").get_wrapped<bool>();
    subcom->add_flag("-l,--long-descriptions",
                     show_long_description.set_cli_config(0),
                     show_long_description.description());

    auto& show_groups = config.at("config_show_groups").get_wrapped<bool>();
    subcom->add_flag("-g,--groups", show_groups.set_cli_config(0), show_groups.description());
}


void
set_config_list_command(CLI::App* subcom)
{
    init_config_list_options(subcom);

    subcom->callback([&]() {
        auto& config = Configuration::instance();

        config.at("show_banner").get_wrapped<bool>().set_value(false);
        config.load(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                    | MAMBA_ALLOW_EXISTING_PREFIX);

        auto& show_sources = config.at("config_show_sources").value<bool>();
        auto& show_all = config.at("config_show_all").value<bool>();
        auto& show_groups = config.at("config_show_groups").value<bool>();
        auto& show_desc = config.at("config_show_descriptions").value<bool>();
        auto& show_long_desc = config.at("config_show_long_descriptions").value<bool>();
        auto& specs = config.at("config_specs").value<std::vector<std::string>>();

        std::cout << config.dump(
            true, show_sources, show_all, show_groups, show_desc, show_long_desc, specs)
                  << std::endl;

        return 0;
    });
}

void
set_config_sources_command(CLI::App* subcom)
{
    init_config_options(subcom);

    subcom->callback([&]() {
        auto& config = Configuration::instance();

        config.at("show_banner").get_wrapped<bool>().set_value(false);
        config.load(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                    | MAMBA_ALLOW_EXISTING_PREFIX);
        auto& no_rc = config.at("no_rc").value<bool>();

        if (no_rc)
        {
            std::cout << "Configuration files disabled by --no-rc flag" << std::endl;
        }
        else
        {
            std::cout << "Configuration files (by precedence order):" << std::endl;

            auto srcs = config.sources();
            auto valid_srcs = config.valid_sources();

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
set_config_describe_command(CLI::App* subcom)
{
    init_config_describe_options(subcom);

    subcom->callback([&]() {
        auto& config = Configuration::instance();

        config.at("show_banner").get_wrapped<bool>().set_value(false);
        config.load(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                    | MAMBA_ALLOW_EXISTING_PREFIX);

        auto& show_groups = config.at("config_show_groups").value<bool>();
        auto& show_long_desc = config.at("config_show_long_descriptions").value<bool>();
        auto& specs = config.at("config_specs").value<std::vector<std::string>>();

        std::cout << config.dump(false, false, true, show_groups, true, show_long_desc, specs)
                  << std::endl;

        return 0;
    });
}

void
set_config_command(CLI::App* subcom)
{
    init_config_options(subcom);

    auto list_subcom = subcom->add_subcommand("list", "List configuration values");
    set_config_list_command(list_subcom);

    auto sources_subcom = subcom->add_subcommand("sources", "Show configuration sources");
    set_config_sources_command(sources_subcom);

    auto describe_subcom
        = subcom->add_subcommand("describe", "Describe given configuration parameters");
    set_config_describe_command(describe_subcom);
}
