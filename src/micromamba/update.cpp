// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "update.hpp"
#include "install.hpp"
#include "options.hpp"
#include "parsers.hpp"

#include "mamba/virtual_packages.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_update_parser(CLI::App* subcom)
{
    init_install_parser(subcom);

    subcom->get_option("specs")->description("Specs to update in the environment");
    subcom->add_flag(
        "-a, --all", update_options.update_all, "Update all packages in the environment");
}


void
set_update_command(CLI::App* subcom)
{
    init_update_parser(subcom);

    subcom->callback([&]() {
        auto& ctx = Context::instance();
        load_install_options(ctx);

        if (update_options.update_all)
        {
            auto& ctx = Context::instance();
            if (ctx.target_prefix.empty())
            {
                throw std::runtime_error(
                    "No active target prefix.\n\nRun $ micromamba activate <PATH_TO_MY_ENV>\nto activate an environment.\n");
            }

            PrefixData prefix_data(ctx.target_prefix);
            prefix_data.load();

            for (const auto& package : prefix_data.m_package_records)
            {
                auto name = package.second.name;
                if (name != "python")
                {
                    create_options.specs.push_back(name);
                }
            }
        }

        if (!create_options.specs.empty())
        {
            install_specs(create_options.specs, false, SOLVER_UPDATE);
        }
        else
        {
            Console::print("Nothing to do.");
        }
    });
}
