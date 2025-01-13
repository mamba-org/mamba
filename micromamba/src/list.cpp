// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/list.hpp"

#include "common_options.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_list_parser(CLI::App* subcom, Configuration& config)
{
    init_general_options(subcom, config);
    init_prefix_options(subcom, config);

    auto& regex = config.insert(Configurable("list_regex", std::string(""))
                                    .group("cli")
                                    .description("List only packages matching a regular expression"));
    subcom->add_option("regex", regex.get_cli_config<std::string>(), regex.description());

    auto& full_name = config.insert(Configurable("full_name", false)
                                        .group("cli")
                                        .description("Only search for full names, i.e., ^<regex>$."));
    subcom->add_flag("-f,--full-name", full_name.get_cli_config<bool>(), full_name.description());

    auto& no_pip = config.insert(Configurable("no_pip", false)
                                     .group("cli")
                                     .description("Do not include pip-only installed packages."));
    subcom->add_flag("--no-pip", no_pip.get_cli_config<bool>(), no_pip.description());

    auto& reverse = config.insert(
        Configurable("reverse", false).group("cli").description("List installed packages in reverse order.")
    );
    subcom->add_flag("--reverse", reverse.get_cli_config<bool>(), reverse.description());
    // TODO: implement this in libmamba/list.cpp
    /*auto& canonical = config.insert(Configurable("canonical", false)
                                        .group("cli")
                                        .description("Output canonical names of packages only."));
    subcom->add_flag("-c,--canonical", canonical.get_cli_config<bool>(), canonical.description());*/
}

void
set_list_command(CLI::App* subcom, Configuration& config)
{
    init_list_parser(subcom, config);

    subcom->callback(
        [&config]
        {
            auto& regex = config.at("list_regex").compute().value<std::string>();
            list(config, regex);
        }
    );
}
