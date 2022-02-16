// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/list.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_list_parser(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);

    auto& config = Configuration::instance();

    auto& regex
        = config.insert(Configurable("list_regex", std::string(""))
                            .group("cli")
                            .description("List only packages matching a regular expression"));
    subcom->add_option("regex", regex.get_cli_config<std::string>(), regex.description());
}

void
set_list_command(CLI::App* subcom)
{
    init_list_parser(subcom);

    subcom->callback([]() {
        auto& config = Configuration::instance();
        auto& regex = config.at("list_regex").compute().value<std::string>();

        list(regex);
    });
}
