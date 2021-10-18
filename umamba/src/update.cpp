// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"
#include "mamba/api/update.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_update_parser(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    init_install_options(subcom);

    auto& update_all = config.insert(Configurable("update_all", false)
                                         .group("cli")
                                         .description("Update all packages in the environment"));

    subcom->get_option("specs")->description("Specs to update in the environment");
    subcom->add_flag("-a, --all", update_all.set_cli_config({}), update_all.description());
}


void
set_update_command(CLI::App* subcom)
{
    init_update_parser(subcom);

    subcom->callback([&]() {
        auto& update_all = Configuration::instance().at("update_all").compute().value<bool>();
        update(update_all);
    });
}
