// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/remove.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_remove_parser(CLI::App* subcom)
{
    init_general_options(subcom);
    init_prefix_options(subcom);

    auto& config = Configuration::instance();

    auto& specs = config.at("specs").get_wrapped<std::vector<std::string>>();
    subcom->add_option("specs", specs.set_cli_config({}), "Specs to remove from the environment");

    auto& remove_all = config.insert(Configurable("remove_all", false)
                                         .group("cli")
                                         .description("Remove all packages in the environment"));
    subcom->add_flag("-a, --all", remove_all.set_cli_config(0), remove_all.description());
}


void
set_remove_command(CLI::App* subcom)
{
    init_remove_parser(subcom);

    subcom->callback([&]() {
        auto& remove_all = Configuration::instance().at("remove_all").compute().value<bool>();
        remove(remove_all);
    });
}
