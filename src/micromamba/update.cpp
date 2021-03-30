// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "update.hpp"
#include "install.hpp"
#include "common_options.hpp"

#include "mamba/configuration.hpp"
#include "mamba/virtual_packages.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_update_parser(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    init_install_parser(subcom);

    auto& update_all = config.insert(Configurable("update_all", false)
                                         .group("cli")
                                         .rc_configurable(false)
                                         .description("Update all packages in the environment"));

    subcom->get_option("specs")->description("Specs to update in the environment");
    subcom->add_flag("-a, --all", update_all.set_cli_config({}), update_all.description());
}


void
set_update_command(CLI::App* subcom)
{
    init_update_parser(subcom);

    subcom->callback([&]() {
        parse_file_options();
        load_configuration(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                           | MAMBA_ALLOW_EXISTING_PREFIX);

        auto& ctx = Context::instance();
        auto& config = Configuration::instance();
        auto update_specs = config.at("specs").value<std::vector<std::string>>();
        auto& update_all = config.at("update_all").value<bool>();

        if (update_all)
        {
            PrefixData prefix_data(ctx.target_prefix);
            prefix_data.load();

            for (const auto& package : prefix_data.m_package_records)
            {
                auto name = package.second.name;
                if (name != "python")
                {
                    update_specs.push_back(name);
                }
            }
        }

        if (!update_specs.empty())
        {
            check_target_prefix(MAMBA_ALLOW_ROOT_PREFIX | MAMBA_ALLOW_FALLBACK_PREFIX
                                | MAMBA_ALLOW_EXISTING_PREFIX);
            install_specs(update_specs, false, SOLVER_UPDATE);
        }
        else
        {
            Console::print("Nothing to do.");
        }
    });
}
