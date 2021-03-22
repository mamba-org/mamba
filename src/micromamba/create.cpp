// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "create.hpp"
#include "install.hpp"
#include "common_options.hpp"

#include "mamba/configuration.hpp"

using namespace mamba;  // NOLINT(build/namespaces)

void
set_create_command(CLI::App* subcom)
{
    init_install_parser(subcom);

    subcom->callback([&]() {
        parse_file_options();
        load_configuration();

        auto& configuration = Configuration::instance();
        auto& specs = configuration.at("specs").value<std::vector<std::string>>();

        if (!specs.empty())
        {
            check_target_prefix(0);
            install_specs(specs, true);
        }
        else
        {
            Console::print("Nothing to do.");
        }

        return 0;
    });
}
