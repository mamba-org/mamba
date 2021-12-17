// Copyright (c) 2019, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#include "common_options.hpp"

#include "mamba/api/configuration.hpp"

#include "termcolor/termcolor.hpp"


using namespace mamba;  // NOLINT(build/namespaces)

void
init_activate_parser(CLI::App* subcom)
{
    auto& config = Configuration::instance();

    auto& env_name = config.insert(Configurable("activate_env_name", std::string(""))
                                       .group("cli")
                                       .description("The name of the environment to activate"));
    subcom->add_option("env_name", env_name.set_cli_config(""), env_name.description());
}

void
set_activate_command(CLI::App* subcom)
{
    init_activate_parser(subcom);

    subcom->callback([&]() {
        std::stringstream message;

        message
            << "\n"
            << "In order to use activate and deactivate you need to initialize your shell. "
            << "(Micromamba is running as a subprocess, so it cannot modify the parent shell.)\n"
            << "To initialize the current (bash) shell, run:\n\n"
            << "    $ eval \"$(micromamba shell hook --shell=bash)\"\n\n"
            << "and then activate or deactivate with:\n\n"
            << "    $ micromamba activate\n\n"
            << "To automatically initialize all future (bash) shells, run:\n\n"
            << "    $ micromamba shell init --shell=bash --prefix=/home/$USER/micromamba \n\n"
            << "Supported shells are bash, zsh, xonsh, cmd.exe, powershell, and fish."
            << termcolor::reset;

        throw std::runtime_error(message.str());
    });
}
