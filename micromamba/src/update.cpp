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
set_update_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    init_install_options(subcom);

    static bool prune = true;
    static bool update_all = false;
    subcom->add_flag("--prune,!--no-prune", prune, "Prune dependencies (default)");

    subcom->get_option("specs")->description("Specs to update in the environment");
    subcom->add_flag("-a, --all", update_all, "Update all packages in the environment");

    subcom->callback([&]() { update(update_all, prune); });
}

void
set_repoquery_command(CLI::App* subcom)
{
    auto& config = Configuration::instance();
    init_general_options(subcom);
    init_prefix_options(subcom);
    init_network_options(subcom);
    init_channel_parser(subcom);

    static std::vector<std::string> specs;
    subcom->add_option("specs", specs, "Specs to search");

    subcom->callback([&]() { repoquery(QueryType::Search, specs[0]); });
}
