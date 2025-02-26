// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "mamba/api/configuration.hpp"
#include "mamba/api/info.hpp"
#include "mamba/core/context.hpp"

#include "common_options.hpp"

void
init_info_parser(CLI::App* subcom, mamba::Configuration& config)
{
    init_general_options(subcom, config);
    init_prefix_options(subcom, config);

    auto& print_licenses = config.insert(
        mamba::Configurable("print_licenses", false).group("cli").description("Print licenses.")
    );
    subcom->add_flag("--licenses", print_licenses.get_cli_config<bool>(), print_licenses.description());

    auto& base = config.insert(
        mamba::Configurable("base", false).group("cli").description("Display base environment path.")
    );
    subcom->add_flag("--base", base.get_cli_config<bool>(), base.description());

    auto& environements = config.insert(
        mamba::Configurable("environements", false).group("cli").description("List known environments.")
    );
    subcom->add_flag("-e,--envs", environements.get_cli_config<bool>(), environements.description());
}

void
set_info_command(CLI::App* subcom, mamba::Configuration& config)
{
    init_info_parser(subcom, config);

    subcom->callback([&config] { info(config); });
}
